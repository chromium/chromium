// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_protocols.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_url_loader.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_type.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/content_verify_job.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/url_request_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/csp_info.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/base/filename_util.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_simple_job.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "url/url_util.h"

using content::BrowserContext;
using content::ResourceRequestInfo;
using extensions::Extension;
using extensions::SharedModuleInfo;

namespace extensions {
namespace {

ExtensionProtocolTestHandler* g_test_handler = nullptr;

void GenerateBackgroundPageContents(const Extension* extension,
                                    std::string* mime_type,
                                    std::string* charset,
                                    std::string* data) {
  *mime_type = "text/html";
  *charset = "utf-8";
  *data = "<!DOCTYPE html>\n<body>\n";
  for (const auto& script : BackgroundInfo::GetBackgroundScripts(extension)) {
    *data += "<script src=\"";
    *data += script;
    *data += "\"></script>\n";
  }
}

class GeneratedBackgroundPageJob : public net::URLRequestSimpleJob {
 public:
  GeneratedBackgroundPageJob(net::URLRequest* request,
                             net::NetworkDelegate* network_delegate,
                             const scoped_refptr<const Extension> extension,
                             const std::string& content_security_policy)
      : net::URLRequestSimpleJob(request, network_delegate),
        extension_(extension) {
    const bool send_cors_headers = false;
    // Leave cache headers out of generated background page jobs.
    response_info_.headers = BuildHttpHeaders(content_security_policy,
                                              send_cors_headers,
                                              base::Time());
  }

  // Overridden from URLRequestSimpleJob:
  int GetData(std::string* mime_type,
              std::string* charset,
              std::string* data,
              net::CompletionOnceCallback callback) const override {
    GenerateBackgroundPageContents(extension_.get(), mime_type, charset, data);
    return net::OK;
  }

  void GetResponseInfo(net::HttpResponseInfo* info) override {
    *info = response_info_;
  }

 private:
  ~GeneratedBackgroundPageJob() override {}

  scoped_refptr<const Extension> extension_;
  net::HttpResponseInfo response_info_;
};

base::Time GetFileLastModifiedTime(const base::FilePath& filename) {
  if (base::PathExists(filename)) {
    base::File::Info info;
    if (base::GetFileInfo(filename, &info))
      return info.last_modified;
  }
  return base::Time();
}

base::Time GetFileCreationTime(const base::FilePath& filename) {
  if (base::PathExists(filename)) {
    base::File::Info info;
    if (base::GetFileInfo(filename, &info))
      return info.creation_time;
  }
  return base::Time();
}

void ReadResourceFilePathAndLastModifiedTime(
    const extensions::ExtensionResource& resource,
    const base::FilePath& directory,
    base::FilePath* file_path,
    base::Time* last_modified_time) {
  // NOTE: ExtensionResource::GetFilePath() must be called on a sequence which
  // tolerates blocking operations.
  *file_path = resource.GetFilePath();
  *last_modified_time = GetFileLastModifiedTime(*file_path);
  // While we're here, log the delta between extension directory
  // creation time and the resource's last modification time.
  base::ElapsedTimer query_timer;
  base::Time dir_creation_time = GetFileCreationTime(directory);
  UMA_HISTOGRAM_TIMES("Extensions.ResourceDirectoryTimestampQueryLatency",
                      query_timer.Elapsed());
  int64_t delta_seconds = (*last_modified_time - dir_creation_time).InSeconds();
  if (delta_seconds >= 0) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Extensions.ResourceLastModifiedDelta",
                                delta_seconds, 1,
                                base::TimeDelta::FromDays(30).InSeconds(), 50);
  } else {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Extensions.ResourceLastModifiedNegativeDelta",
                                -delta_seconds,
                                1,
                                base::TimeDelta::FromDays(30).InSeconds(),
                                50);
  }
}

class URLRequestExtensionJob : public net::URLRequestFileJob {
 public:
  URLRequestExtensionJob(net::URLRequest* request,
                         net::NetworkDelegate* network_delegate,
                         const std::string& extension_id,
                         const base::FilePath& directory_path,
                         const base::FilePath& relative_path,
                         const std::string& content_security_policy,
                         bool send_cors_header,
                         bool follow_symlinks_anywhere,
                         scoped_refptr<ContentVerifyJob> verify_job)
      : net::URLRequestFileJob(
            request,
            network_delegate,
            base::FilePath(),
            base::CreateTaskRunnerWithTraits(
                {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                 base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
        verify_job_(std::move(verify_job)),
        seek_position_(0),
        bytes_read_(0),
        directory_path_(directory_path),
        // TODO(tc): Move all of these files into resources.pak so we don't
        // break when updating on Linux.
        resource_(extension_id, directory_path, relative_path),
        content_security_policy_(content_security_policy),
        send_cors_header_(send_cors_header),
        weak_factory_(this) {
    if (follow_symlinks_anywhere) {
      resource_.set_follow_symlinks_anywhere();
    }
  }

  void GetResponseInfo(net::HttpResponseInfo* info) override {
    *info = response_info_;
  }

  void Start() override {
    request_timer_.reset(new base::ElapsedTimer());
    base::FilePath* read_file_path = new base::FilePath;
    base::Time* last_modified_time = new base::Time();

    // Inherit task priority from the calling context.
    base::PostTaskWithTraitsAndReply(
        FROM_HERE, {base::MayBlock()},
        base::Bind(&ReadResourceFilePathAndLastModifiedTime, resource_,
                   directory_path_, base::Unretained(read_file_path),
                   base::Unretained(last_modified_time)),
        base::Bind(&URLRequestExtensionJob::OnFilePathAndLastModifiedTimeRead,
                   weak_factory_.GetWeakPtr(), base::Owned(read_file_path),
                   base::Owned(last_modified_time)));
  }

  bool IsRedirectResponse(GURL* location,
                          int* http_status_code,
                          bool* insecure_scheme_was_upgraded) override {
    return false;
  }

  void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers) override {
    // TODO(asargent) - we'll need to add proper support for range headers.
    // crbug.com/369895.
    std::string range_header;
    if (headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header)) {
      if (verify_job_.get())
        verify_job_ = NULL;
    }
    URLRequestFileJob::SetExtraRequestHeaders(headers);
  }

  void OnOpenComplete(int result) override {
    if (result < 0) {
      // This can happen when the file is unreadable (which can happen during
      // corruption or third-party interaction). We need to be sure to inform
      // the verification job that we've finished reading so that it can
      // proceed; see crbug.com/703888.
      if (verify_job_.get()) {
        std::string tmp;
        verify_job_->BytesRead(0, base::data(tmp));
        verify_job_->DoneReading();
      }
    }
  }

  void OnSeekComplete(int64_t result) override {
    DCHECK_EQ(seek_position_, 0);
    seek_position_ = result;
    // TODO(asargent) - we'll need to add proper support for range headers.
    // crbug.com/369895.
    if (result > 0 && verify_job_.get())
      verify_job_ = NULL;
  }

  void OnReadComplete(net::IOBuffer* buffer, int result) override {
    if (result >= 0)
      UMA_HISTOGRAM_COUNTS_1M("ExtensionUrlRequest.OnReadCompleteResult",
                              result);
    else
      base::UmaHistogramSparse("ExtensionUrlRequest.OnReadCompleteError",
                               -result);
    if (result > 0) {
      bytes_read_ += result;
      if (verify_job_.get())
        verify_job_->BytesRead(result, buffer->data());
    }
  }

  void DoneReading() override {
    URLRequestFileJob::DoneReading();
    if (verify_job_.get())
      verify_job_->DoneReading();
  }

 private:
  ~URLRequestExtensionJob() override {
    UMA_HISTOGRAM_COUNTS_1M("ExtensionUrlRequest.TotalKbRead",
                            bytes_read_ / 1024);
    UMA_HISTOGRAM_COUNTS_1M("ExtensionUrlRequest.SeekPosition", seek_position_);
    if (request_timer_.get())
      UMA_HISTOGRAM_TIMES("ExtensionUrlRequest.Latency",
                          request_timer_->Elapsed());
  }

  bool CanAccessFile(const base::FilePath& original_path,
                     const base::FilePath& absolute_path) override {
    // The access checks for the file are performed before the job is
    // created, so we should know that this is safe.
    return true;
  }

  void OnFilePathAndLastModifiedTimeRead(base::FilePath* read_file_path,
                                         base::Time* last_modified_time) {
    file_path_ = *read_file_path;
    response_info_.headers = BuildHttpHeaders(
        content_security_policy_,
        send_cors_header_,
        *last_modified_time);
    // Set the mime type for the request.
    std::string mime_type;
    bool found_mime_type = GetMimeType(&mime_type);
    if (found_mime_type)
      response_info_.headers->AddHeader("Content-Type: " + mime_type);

    URLRequestFileJob::Start();
  }

  bool GetMimeType(std::string* mime_type) const override {
    base::FilePath::StringType file_extension = file_path_.Extension();
    if (file_extension.empty())
      return false;

    // We use GetWellKnownMimeTypeFromExtension() to ensure that configurations
    // that may have been set by other programs on a user's machine don't affect
    // the mime type returned (in particular, JS should always be
    // application/javascript). See https://crbug.com/797712. Using an accurate
    // mime type is necessary at least for modules, which enforce strict mime
    // type requirements.
    return net::GetWellKnownMimeTypeFromExtension(
        file_extension.substr(1),  // Trim leading '.'
        mime_type);
  }

  scoped_refptr<ContentVerifyJob> verify_job_;

  std::unique_ptr<base::ElapsedTimer> request_timer_;

  // The position we seeked to in the file.
  int64_t seek_position_;

  // The number of bytes of content we read from the file.
  int bytes_read_;

  net::HttpResponseInfo response_info_;
  base::FilePath directory_path_;
  extensions::ExtensionResource resource_;
  std::string content_security_policy_;
  bool send_cors_header_;
  base::WeakPtrFactory<URLRequestExtensionJob> weak_factory_;
};

bool ExtensionCanLoadInIncognito(bool is_main_frame,
                                 const Extension* extension,
                                 bool extension_enabled_in_incognito) {
  if (!extension || !extension_enabled_in_incognito)
    return false;
  if (!is_main_frame)
    return true;

  // Only allow incognito toplevel navigations to extension resources in
  // split mode. In spanning mode, the extension must run in a single process,
  // and an incognito tab prevents that.
  return IncognitoInfo::IsSplitMode(extension);
}

// Returns true if an chrome-extension:// resource should be allowed to load.
// Pass true for |is_incognito| only for incognito profiles and not Chrome OS
// guest mode profiles.
//
// May be called on the IO thread (non-Network Service path) or the UI thread
// (Network Service path).
bool AllowExtensionResourceLoad(const GURL& url,
                                content::ResourceType resource_type,
                                ui::PageTransition page_transition,
                                int child_id,
                                bool is_incognito,
                                const Extension* extension,
                                bool extension_enabled_in_incognito,
                                const ExtensionSet& extensions,
                                const ProcessMap& process_map) {
  const bool is_main_frame = resource_type == content::RESOURCE_TYPE_MAIN_FRAME;
  if (is_incognito &&
      !ExtensionCanLoadInIncognito(is_main_frame, extension,
                                   extension_enabled_in_incognito)) {
    return false;
  }

  // The following checks are meant to replicate similar set of checks in the
  // renderer process, performed by ResourceRequestPolicy::CanRequestResource.
  // These are not exactly equivalent, because we don't have the same bits of
  // information. The two checks need to be kept in sync as much as possible, as
  // an exploited renderer can bypass the checks in ResourceRequestPolicy.

  // Check if the extension for which this request is made is indeed loaded in
  // the process sending the request. If not, we need to explicitly check if
  // the resource is explicitly accessible or fits in a set of exception cases.
  // Note: This allows a case where two extensions execute in the same renderer
  // process to request each other's resources. We can't do a more precise
  // check, since the renderer can lie about which extension has made the
  // request.
  if (process_map.Contains(url.host(), child_id))
    return true;

  // Frame navigations to extensions have already been checked in
  // the ExtensionNavigationThrottle.
  if (child_id == -1 && content::IsResourceTypeFrame(resource_type))
    return true;

  // Allow the extension module embedder to grant permission for loads.
  if (ExtensionsBrowserClient::Get()->AllowCrossRendererResourceLoad(
          url, resource_type, page_transition, child_id, is_incognito,
          extension, extensions, process_map)) {
    return true;
  }

  // No special exceptions for cross-process loading. Block the load.
  return false;
}

// Returns true if the given URL references an icon in the given extension.
bool URLIsForExtensionIcon(const GURL& url, const Extension* extension) {
  DCHECK(url.SchemeIs(extensions::kExtensionScheme));
  if (!extension)
    return false;

  DCHECK_EQ(url.host(), extension->id());
  base::StringPiece path = url.path_piece();
  DCHECK(path.length() > 0 && path[0] == '/');
  base::StringPiece path_without_slash = path.substr(1);
  return IconsInfo::GetIcons(extension).ContainsPath(path_without_slash);
}

// Retrieves the path corresponding to an extension on disk. Returns |true| on
// success and populates |*path|; otherwise returns |false|.
bool GetDirectoryForExtensionURL(const GURL& url,
                                 const std::string& extension_id,
                                 const Extension* extension,
                                 const ExtensionSet& disabled_extensions,
                                 base::FilePath* out_path) {
  base::FilePath path;
  if (extension)
    path = extension->path();
  const Extension* disabled_extension =
      disabled_extensions.GetByID(extension_id);
  if (path.empty()) {
    // For disabled extensions, we only resolve the directory path to service
    // extension icon URL requests.
    if (URLIsForExtensionIcon(url, disabled_extension))
      path = disabled_extension->path();
  }

  if (!path.empty()) {
    *out_path = path;
    return true;
  }

  DLOG_IF(WARNING, !disabled_extension)
      << "Failed to get directory for extension " << extension_id;

  return false;
}

bool IsWebViewRequest(net::URLRequest* request) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  // |info| can be null sometimes: http://crbug.com/370070.
  if (!info)
    return false;
  if (WebViewRendererState::GetInstance()->IsGuest(info->GetChildID()))
    return true;

  // GetChildId() is -1 with PlzNavigate for navigation requests, so also try
  // the ExtensionNavigationUIData data.
  const ExtensionNavigationUIData* data =
      ExtensionsBrowserClient::Get()->GetExtensionNavigationUIData(request);
  return data && data->is_web_view();
}

void GetSecurityPolicyForURL(const GURL& url,
                             const Extension* extension,
                             bool is_web_view_request,
                             std::string* content_security_policy,
                             bool* send_cors_header,
                             bool* follow_symlinks_anywhere) {
  std::string resource_path = url.path();

  // Use default CSP for <webview>.
  if (!is_web_view_request) {
    *content_security_policy =
        extensions::CSPInfo::GetResourceContentSecurityPolicy(extension,
                                                              resource_path);
  }

  if (extensions::WebAccessibleResourcesInfo::IsResourceWebAccessible(
          extension, resource_path)) {
    *send_cors_header = true;
  }

  *follow_symlinks_anywhere =
      (extension->creation_flags() & Extension::FOLLOW_SYMLINKS_ANYWHERE) != 0;
}

bool IsBackgroundPageURL(const GURL& url) {
  std::string path = url.path();
  return path.size() > 1 && path.substr(1) == kGeneratedBackgroundPageFilename;
}

class ExtensionProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  ExtensionProtocolHandler(bool is_incognito,
                           extensions::InfoMap* extension_info_map)
      : is_incognito_(is_incognito), extension_info_map_(extension_info_map) {}

  ~ExtensionProtocolHandler() override {}

  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

  bool IsSafeRedirectTarget(const GURL& location) const override;

 private:
  const bool is_incognito_;
  extensions::InfoMap* const extension_info_map_;
  DISALLOW_COPY_AND_ASSIGN(ExtensionProtocolHandler);
};

// Creates URLRequestJobs for extension:// URLs.
net::URLRequestJob*
ExtensionProtocolHandler::MaybeCreateJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  // chrome-extension://extension-id/resource/path.js
  std::string extension_id = request->url().host();
  const Extension* extension =
      extension_info_map_->extensions().GetByID(extension_id);
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  const bool enabled_in_incognito =
      extension_info_map_->IsIncognitoEnabled(extension_id);

  // We have seen crashes where info is NULL: crbug.com/52374.
  if (!info) {
    // SeviceWorker net requests created through ServiceWorkerWriteToCacheJob
    // do not have ResourceRequestInfo associated with them. So skip logging
    // spurious errors below.
    // TODO(falken): Either consider attaching ResourceRequestInfo to these or
    // finish refactoring ServiceWorkerWriteToCacheJob so that it doesn't spawn
    // a new URLRequest.
    if (!ResourceRequestInfo::OriginatedFromServiceWorker(request)) {
      LOG(ERROR) << "Allowing load of " << request->url().spec()
                 << "from unknown origin. Could not find user data for "
                 << "request.";
    }
  } else if (!AllowExtensionResourceLoad(
                 request->url(), info->GetResourceType(),
                 info->GetPageTransition(), info->GetChildID(), is_incognito_,
                 extension, enabled_in_incognito,
                 extension_info_map_->extensions(),
                 extension_info_map_->process_map())) {
    return new net::URLRequestErrorJob(request, network_delegate,
                                       net::ERR_BLOCKED_BY_CLIENT);
  }

  base::FilePath directory_path;
  if (!GetDirectoryForExtensionURL(request->url(), extension_id, extension,
                                   extension_info_map_->disabled_extensions(),
                                   &directory_path)) {
    return nullptr;
  }

  // Set up content security policy.
  std::string content_security_policy;
  bool send_cors_header = false;
  bool follow_symlinks_anywhere = false;
  if (extension) {
    GetSecurityPolicyForURL(request->url(), extension,
                            IsWebViewRequest(request), &content_security_policy,
                            &send_cors_header, &follow_symlinks_anywhere);
  }

  // Create a job for a generated background page.
  if (IsBackgroundPageURL(request->url())) {
    return new GeneratedBackgroundPageJob(
        request, network_delegate, extension, content_security_policy);
  }

  // Component extension resources may be part of the embedder's resource files,
  // for example component_extension_resources.pak in Chrome.
  net::URLRequestJob* resource_bundle_job =
      extensions::ExtensionsBrowserClient::Get()
          ->MaybeCreateResourceBundleRequestJob(request,
                                                network_delegate,
                                                directory_path,
                                                content_security_policy,
                                                send_cors_header);
  if (resource_bundle_job)
    return resource_bundle_job;

  base::FilePath relative_path =
      extensions::file_util::ExtensionURLToRelativeFilePath(request->url());

  // Do not allow requests for resources in the _metadata folder, since any
  // files there are internal implementation details that should not be
  // considered part of the extension.
  if (base::FilePath(kMetadataFolder).IsParent(relative_path))
    return nullptr;

  // Handle shared resources (extension A loading resources out of extension B).
  std::string path = request->url().path();
  if (SharedModuleInfo::IsImportedPath(path)) {
    std::string new_extension_id;
    std::string new_relative_path;
    SharedModuleInfo::ParseImportedPath(path, &new_extension_id,
                                        &new_relative_path);
    const Extension* new_extension =
        extension_info_map_->extensions().GetByID(new_extension_id);

    if (SharedModuleInfo::ImportsExtensionById(extension, new_extension_id) &&
        new_extension) {
      directory_path = new_extension->path();
      extension_id = new_extension_id;
      relative_path = base::FilePath::FromUTF8Unsafe(new_relative_path);
    } else {
      return NULL;
    }
  }

  if (g_test_handler)
    g_test_handler->Run(&directory_path, &relative_path);

  scoped_refptr<ContentVerifyJob> verify_job;
  ContentVerifier* verifier = extension_info_map_->content_verifier();
  if (verifier) {
    verify_job =
        verifier->CreateJobFor(extension_id, directory_path, relative_path);
    if (verify_job)
      verify_job->Start(verifier);
  }

  return new URLRequestExtensionJob(
      request, network_delegate, extension_id, directory_path, relative_path,
      content_security_policy, send_cors_header, follow_symlinks_anywhere,
      std::move(verify_job));
}

bool ExtensionProtocolHandler::IsSafeRedirectTarget(
    const GURL& location) const {
  // Redirects originate from http/https origins, which are subject to
  // webaccessible resources restrictions. Redirects can also be triggered via
  // the extensions webrequest API, which is also limited to webaccessible
  // resources.
  std::string extension_id = location.host();
  const Extension* extension =
      extension_info_map_->extensions().GetByID(extension_id);
  if (!extension)
    return false;
  return WebAccessibleResourcesInfo::IsResourceWebAccessible(extension,
                                                             location.path());
}

class FileLoaderObserver : public content::FileURLLoaderObserver {
 public:
  explicit FileLoaderObserver(scoped_refptr<ContentVerifyJob> verify_job)
      : verify_job_(std::move(verify_job)) {}
  ~FileLoaderObserver() override {
    base::AutoLock auto_lock(lock_);
    UMA_HISTOGRAM_COUNTS_1M("ExtensionUrlRequest.TotalKbRead",
                            bytes_read_ / 1024);
    UMA_HISTOGRAM_COUNTS_1M("ExtensionUrlRequest.SeekPosition", seek_position_);
    if (request_timer_.get())
      UMA_HISTOGRAM_TIMES("ExtensionUrlRequest.Latency",
                          request_timer_->Elapsed());
  }

  void OnStart() override {
    base::AutoLock auto_lock(lock_);
    request_timer_.reset(new base::ElapsedTimer());
  }

  void OnSeekComplete(int64_t result) override {
    DCHECK_EQ(seek_position_, 0);
    base::AutoLock auto_lock(lock_);
    seek_position_ = result;
    // TODO(asargent) - we'll need to add proper support for range headers.
    // crbug.com/369895.
    if (result > 0 && verify_job_.get())
      verify_job_ = nullptr;
  }

  void OnBytesRead(const void* data,
                   size_t num_bytes_read,
                   base::File::Error read_result) override {
    if (read_result == base::File::FILE_OK) {
      UMA_HISTOGRAM_COUNTS_1M("ExtensionUrlRequest.OnReadCompleteResult",
                              read_result);
      base::AutoLock auto_lock(lock_);
      bytes_read_ += num_bytes_read;
      if (verify_job_.get())
        verify_job_->BytesRead(num_bytes_read, static_cast<const char*>(data));
    } else {
      net::Error net_error = net::FileErrorToNetError(read_result);
      base::UmaHistogramSparse("ExtensionUrlRequest.OnReadCompleteError",
                               net_error);
    }
  }

  void OnDoneReading() override {
    base::AutoLock auto_lock(lock_);
    if (verify_job_.get())
      verify_job_->DoneReading();
  }

 private:
  int64_t bytes_read_ = 0;
  int64_t seek_position_ = 0;
  std::unique_ptr<base::ElapsedTimer> request_timer_;
  scoped_refptr<ContentVerifyJob> verify_job_;
  // To synchronize access to all members.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(FileLoaderObserver);
};

class ExtensionURLLoaderFactory : public network::mojom::URLLoaderFactory {
 public:
  ExtensionURLLoaderFactory(int render_process_id, int render_frame_id)
      : render_process_id_(render_process_id) {
    content::RenderProcessHost* process_host =
        content::RenderProcessHost::FromID(render_process_id);
    browser_context_ = process_host->GetBrowserContext();
    is_web_view_request_ = WebViewGuest::FromFrameID(
                               render_process_id_, render_frame_id) != nullptr;
    Init();
  }

  ExtensionURLLoaderFactory(content::BrowserContext* browser_context,
                            bool is_web_view_request)
      : browser_context_(browser_context),
        is_web_view_request_(is_web_view_request),
        render_process_id_(-1) {
    Init();
  }

  void Init() {
    extension_info_map_ =
        extensions::ExtensionSystem::Get(browser_context_)->info_map();
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  ~ExtensionURLLoaderFactory() override = default;

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    const std::string extension_id = request.url.host();
    ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
    scoped_refptr<const Extension> extension =
        registry->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);
    const ExtensionSet& enabled_extensions = registry->enabled_extensions();
    const ProcessMap* process_map = ProcessMap::Get(browser_context_);
    bool incognito_enabled =
        extensions::util::IsIncognitoEnabled(extension_id, browser_context_);

    if (!AllowExtensionResourceLoad(
            request.url,
            static_cast<content::ResourceType>(request.resource_type),
            static_cast<ui::PageTransition>(request.transition_type),
            render_process_id_, browser_context_->IsOffTheRecord(),
            extension.get(), incognito_enabled, enabled_extensions,
            *process_map)) {
      client->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_BLOCKED_BY_CLIENT));
      return;
    }

    base::FilePath directory_path;
    if (!GetDirectoryForExtensionURL(request.url, extension_id, extension.get(),
                                     registry->disabled_extensions(),
                                     &directory_path)) {
      client->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }

    LoadExtension(std::move(loader), request, std::move(client), extension,
                  std::move(directory_path));
  }

  void Clone(network::mojom::URLLoaderFactoryRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

 private:
  void LoadExtension(network::mojom::URLLoaderRequest loader,
                     const network::ResourceRequest& request,
                     network::mojom::URLLoaderClientPtr client,
                     scoped_refptr<const Extension> extension,
                     base::FilePath directory_path) {
    // Set up content security policy.
    std::string content_security_policy;
    bool send_cors_header = false;
    bool follow_symlinks_anywhere = false;
    if (extension) {
      GetSecurityPolicyForURL(request.url, extension.get(),
                              is_web_view_request_, &content_security_policy,
                              &send_cors_header, &follow_symlinks_anywhere);
    }

    if (IsBackgroundPageURL(request.url)) {
      // Handle background page requests immediately with a simple generated
      // chunk of HTML.

      // Leave cache headers out of generated background page jobs.
      network::ResourceResponseHead head;
      const bool send_cors_headers = false;
      head.headers = BuildHttpHeaders(content_security_policy,
                                      send_cors_headers, base::Time());
      std::string contents;
      GenerateBackgroundPageContents(extension.get(), &head.mime_type,
                                     &head.charset, &contents);
      uint32_t size = base::saturated_cast<uint32_t>(contents.size());
      mojo::DataPipe pipe(size);
      MojoResult result = pipe.producer_handle->WriteData(
          contents.data(), &size, MOJO_WRITE_DATA_FLAG_NONE);
      if (result != MOJO_RESULT_OK || size < contents.size()) {
        client->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
        return;
      }

      client->OnReceiveResponse(head);
      client->OnStartLoadingResponseBody(std::move(pipe.consumer_handle));
      client->OnComplete(network::URLLoaderCompletionStatus(net::OK));
      return;
    }

    // Component extension resources may be part of the embedder's resource
    // files, for example component_extension_resources.pak in Chrome.
    int resource_id = 0;
    const base::FilePath bundle_resource_path =
        ExtensionsBrowserClient::Get()->GetBundleResourcePath(
            request, directory_path, &resource_id);
    if (!bundle_resource_path.empty()) {
      ExtensionsBrowserClient::Get()->LoadResourceFromResourceBundle(
          request, std::move(loader), bundle_resource_path, resource_id,
          content_security_policy, std::move(client), send_cors_header);
      return;
    }

    base::FilePath relative_path =
        file_util::ExtensionURLToRelativeFilePath(request.url);

    // Do not allow requests for resources in the _metadata folder, since any
    // files there are internal implementation details that should not be
    // considered part of the extension.
    if (base::FilePath(kMetadataFolder).IsParent(relative_path)) {
      client->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_FILE_NOT_FOUND));
      return;
    }

    // Handle shared resources (extension A loading resources out of extension
    // B).
    std::string extension_id = extension->id();
    std::string path = request.url.path();
    if (SharedModuleInfo::IsImportedPath(path)) {
      std::string new_extension_id;
      std::string new_relative_path;
      SharedModuleInfo::ParseImportedPath(path, &new_extension_id,
                                          &new_relative_path);
      ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
      const Extension* new_extension =
          registry->enabled_extensions().GetByID(new_extension_id);
      if (SharedModuleInfo::ImportsExtensionById(extension.get(),
                                                 new_extension_id) &&
          new_extension) {
        directory_path = new_extension->path();
        extension_id = new_extension_id;
        relative_path = base::FilePath::FromUTF8Unsafe(new_relative_path);
      } else {
        client->OnComplete(
            network::URLLoaderCompletionStatus(net::ERR_BLOCKED_BY_CLIENT));
        return;
      }
    }

    if (g_test_handler)
      g_test_handler->Run(&directory_path, &relative_path);

    extensions::ExtensionResource resource(extension_id, directory_path,
                                           relative_path);
    if (follow_symlinks_anywhere)
      resource.set_follow_symlinks_anywhere();

    base::FilePath* read_file_path = new base::FilePath;
    base::Time* last_modified_time = new base::Time();

    scoped_refptr<ContentVerifier> content_verifier =
        extension_info_map_->content_verifier();
    base::PostTaskWithTraitsAndReply(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&ReadResourceFilePathAndLastModifiedTime, resource,
                       directory_path, base::Unretained(read_file_path),
                       base::Unretained(last_modified_time)),
        base::BindOnce(
            &OnFilePathAndLastModifiedTimeRead, base::Owned(read_file_path),
            base::Owned(last_modified_time), request, std::move(loader),
            std::move(client), std::move(content_verifier), resource,
            content_security_policy, send_cors_header));
  }

  static void OnFilePathAndLastModifiedTimeRead(
      const base::FilePath* read_file_path,
      const base::Time* last_modified_time,
      network::ResourceRequest request,
      network::mojom::URLLoaderRequest loader,
      network::mojom::URLLoaderClientPtr client,
      scoped_refptr<ContentVerifier> content_verifier,
      const extensions::ExtensionResource& resource,
      const std::string& content_security_policy,
      bool send_cors_header) {
    request.url = net::FilePathToFileURL(*read_file_path);

    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(
            &StartVerifyJob, std::move(request), std::move(loader),
            std::move(client), std::move(content_verifier), resource,
            BuildHttpHeaders(content_security_policy, send_cors_header,
                             *last_modified_time)));
  }

  static void StartVerifyJob(
      network::ResourceRequest request,
      network::mojom::URLLoaderRequest loader,
      network::mojom::URLLoaderClientPtr client,
      scoped_refptr<ContentVerifier> content_verifier,
      const ExtensionResource& resource,
      scoped_refptr<net::HttpResponseHeaders> response_headers) {
    scoped_refptr<ContentVerifyJob> verify_job;
    if (content_verifier) {
      verify_job = content_verifier->CreateJobFor(resource.extension_id(),
                                                  resource.extension_root(),
                                                  resource.relative_path());
      if (verify_job)
        verify_job->Start(content_verifier.get());
    }

    content::CreateFileURLLoader(
        request, std::move(loader), std::move(client),
        std::make_unique<FileLoaderObserver>(std::move(verify_job)),
        std::move(response_headers));
  }

  content::BrowserContext* browser_context_;
  bool is_web_view_request_;
  // We store the ID and get RenderProcessHost each time it's needed. This is to
  // avoid holding on to stale pointers if we get requests past the lifetime of
  // the objects.
  const int render_process_id_;
  scoped_refptr<extensions::InfoMap> extension_info_map_;
  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionURLLoaderFactory);
};

}  // namespace

scoped_refptr<net::HttpResponseHeaders> BuildHttpHeaders(
    const std::string& content_security_policy,
    bool send_cors_header,
    const base::Time& last_modified_time) {
  std::string raw_headers;
  raw_headers.append("HTTP/1.1 200 OK");
  if (!content_security_policy.empty()) {
    raw_headers.append(1, '\0');
    raw_headers.append("Content-Security-Policy: ");
    raw_headers.append(content_security_policy);
  }

  if (send_cors_header) {
    raw_headers.append(1, '\0');
    raw_headers.append("Access-Control-Allow-Origin: *");
  }

  if (!last_modified_time.is_null()) {
    // Hash the time and make an etag to avoid exposing the exact
    // user installation time of the extension.
    std::string hash =
        base::StringPrintf("%" PRId64, last_modified_time.ToInternalValue());
    hash = base::SHA1HashString(hash);
    std::string etag;
    base::Base64Encode(hash, &etag);
    raw_headers.append(1, '\0');
    raw_headers.append("ETag: \"");
    raw_headers.append(etag);
    raw_headers.append("\"");
    // Also force revalidation.
    raw_headers.append(1, '\0');
    raw_headers.append("cache-control: no-cache");
  }

  raw_headers.append(2, '\0');
  return new net::HttpResponseHeaders(raw_headers);
}

std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>
CreateExtensionProtocolHandler(bool is_incognito,
                               extensions::InfoMap* extension_info_map) {
  return std::make_unique<ExtensionProtocolHandler>(is_incognito,
                                                    extension_info_map);
}

void SetExtensionProtocolTestHandler(ExtensionProtocolTestHandler* handler) {
  g_test_handler = handler;
}

std::unique_ptr<network::mojom::URLLoaderFactory>
CreateExtensionNavigationURLLoaderFactory(
    content::BrowserContext* browser_context,
    bool is_web_view_request) {
  return std::make_unique<ExtensionURLLoaderFactory>(browser_context,
                                                     is_web_view_request);
}

std::unique_ptr<network::mojom::URLLoaderFactory>
CreateExtensionURLLoaderFactory(int render_process_id, int render_frame_id) {
  return std::make_unique<ExtensionURLLoaderFactory>(render_process_id,
                                                     render_frame_id);
}

}  // namespace extensions
