// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_CLIENT_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_CLIENT_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "ui/base/layout.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace base {
class DictionaryValue;
class RefCountedMemory;
class SequencedTaskRunner;
}

namespace blink {
class OriginTrialPolicy;
}

namespace IPC {
class Message;
}

namespace gfx {
class Image;
}

namespace gpu {
struct GPUInfo;
}

namespace media {
struct CdmHostFilePath;
class MediaDrmBridgeClient;
}

namespace mojo {
class BinderMap;
}

namespace content {

class ContentBrowserClient;
class ContentClient;
class ContentGpuClient;
class ContentRendererClient;
class ContentUtilityClient;
struct CdmInfo;
struct PepperPluginInfo;

// Setter and getter for the client.  The client should be set early, before any
// content code is called.
CONTENT_EXPORT void SetContentClient(ContentClient* client);

#if defined(CONTENT_IMPLEMENTATION)
// Content's embedder API should only be used by content.
ContentClient* GetContentClient();
#endif

// Used for tests to override the relevant embedder interfaces. Each method
// returns the old value. In browser tests it seems safest to call these in
// SetUpOnMainThread() or you may get TSan errors due a race between the
// browser "process" and the child "process" for the test both accessing it.
CONTENT_EXPORT ContentBrowserClient* SetBrowserClientForTesting(
    ContentBrowserClient* b);
CONTENT_EXPORT ContentRendererClient* SetRendererClientForTesting(
    ContentRendererClient* r);
CONTENT_EXPORT ContentUtilityClient* SetUtilityClientForTesting(
    ContentUtilityClient* u);

// Interface that the embedder implements.
class CONTENT_EXPORT ContentClient {
 public:
  ContentClient();
  virtual ~ContentClient();

  ContentBrowserClient* browser() { return browser_; }
  ContentGpuClient* gpu() { return gpu_; }
  ContentRendererClient* renderer() { return renderer_; }
  ContentUtilityClient* utility() { return utility_; }

  // Sets the active URL (the URL of a frame that is navigating or processing an
  // IPC message), and the origin of the main frame (for diagnosing crashes).
  // Use GURL() or std::string() to clear the URL/origin.
  //
  // A string is used for the origin because the source of that value may be a
  // WebSecurityOrigin or a full URL (if called from the browser process) and a
  // string is the lowest-common-denominator.
  virtual void SetActiveURL(const GURL& url, std::string top_origin) {}

  // Sets the data on the current gpu.
  virtual void SetGpuInfo(const gpu::GPUInfo& gpu_info) {}

  // Gives the embedder a chance to register its own pepper plugins.
  virtual void AddPepperPlugins(
      std::vector<content::PepperPluginInfo>* plugins) {}

  // Gives the embedder a chance to register the Content Decryption Modules
  // (CDM) it supports, as well as the CDM host file paths to verify CDM host.
  // |cdms| or |cdm_host_file_paths| can be null which means that specific list
  // is not needed.
  virtual void AddContentDecryptionModules(
      std::vector<content::CdmInfo>* cdms,
      std::vector<media::CdmHostFilePath>* cdm_host_file_paths) {}

  // Gives the embedder a chance to register its own schemes early in the
  // startup sequence.
  struct CONTENT_EXPORT Schemes {
    Schemes();
    ~Schemes();
    std::vector<std::string> standard_schemes;
    std::vector<std::string> referrer_schemes;
    std::vector<std::string> savable_schemes;
    // Additional schemes that should be allowed to register service workers.
    // Only secure and trustworthy schemes should be added.
    std::vector<std::string> service_worker_schemes;
    // Registers a URL scheme to be treated as a local scheme (i.e., with the
    // same security rules as those applied to "file" URLs). This means that
    // normal pages cannot link to or access URLs of this scheme.
    std::vector<std::string> local_schemes;
    // Registers a URL scheme to be treated as a noAccess scheme. This means
    // that pages loaded with this URL scheme always have an opaque origin.
    std::vector<std::string> no_access_schemes;
    // Registers a non-HTTP URL scheme which can be sent CORS requests.
    std::vector<std::string> cors_enabled_schemes;
    // Registers a URL scheme whose resources can be loaded regardless of a
    // page's Content Security Policy.
    std::vector<std::string> csp_bypassing_schemes;
    // See https://www.w3.org/TR/powerful-features/#is-origin-trustworthy.
    std::vector<std::string> secure_schemes;
    // Registers a URL scheme as strictly empty documents, allowing them to
    // commit synchronously.
    std::vector<std::string> empty_document_schemes;
#if defined(OS_ANDROID)
    // Normally, non-standard schemes canonicalize to opaque origins. However,
    // Android WebView requires non-standard schemes to still be preserved.
    bool allow_non_standard_schemes_in_origins = false;
#endif
  };

  virtual void AddAdditionalSchemes(Schemes* schemes) {}

  // Returns whether the given message should be sent in a swapped out renderer.
  virtual bool CanSendWhileSwappedOut(const IPC::Message* message);

  // Returns a string resource given its id.
  virtual base::string16 GetLocalizedString(int message_id);

  // Returns a string resource given its id and replace $1 with the given
  // replacement.
  virtual base::string16 GetLocalizedString(int message_id,
                                            const base::string16& replacement);

  // Return the contents of a resource in a StringPiece given the resource id.
  virtual base::StringPiece GetDataResource(int resource_id,
                                            ui::ScaleFactor scale_factor);

  // Returns the raw bytes of a scale independent data resource.
  virtual base::RefCountedMemory* GetDataResourceBytes(int resource_id);

  // Returns a native image given its id.
  virtual gfx::Image& GetNativeImageNamed(int resource_id);

  // Called by content::GetProcessTypeNameInEnglish for process types that it
  // doesn't know about because they're from the embedder.
  virtual std::string GetProcessTypeNameInEnglish(int type);

  // Called once during initialization of NetworkService to provide constants
  // to NetLog.  (Though it may be called multiples times if NetworkService
  // crashes and needs to be reinitialized).  The return value is merged with
  // |GetNetConstants()| and passed to FileNetLogObserver - see documentation
  // of |FileNetLogObserver::CreateBounded()| for more information.  The
  // convention is to put new constants under a subdict at the key "clientInfo".
  virtual base::DictionaryValue GetNetLogConstants();

  // Returns whether or not V8 script extensions should be allowed for a
  // service worker.
  virtual bool AllowScriptExtensionForServiceWorker(
      const url::Origin& script_origin);

  // Returns the origin trial policy, or nullptr if origin trials are not
  // supported by the embedder.
  virtual blink::OriginTrialPolicy* GetOriginTrialPolicy();

#if defined(OS_ANDROID)
  // Returns true for clients like Android WebView that uses synchronous
  // compositor. Note setting this to true will permit synchronous IPCs from
  // the browser UI thread.
  virtual bool UsingSynchronousCompositing();

  // Returns the MediaDrmBridgeClient to be used by media code on Android.
  virtual media::MediaDrmBridgeClient* GetMediaDrmBridgeClient();
#endif  // OS_ANDROID

  // Allows the embedder to handle incoming interface binding requests from
  // the browser process to any type of child process. This is called once
  // in each child process during that process's initialization.
  virtual void ExposeInterfacesToBrowser(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      mojo::BinderMap* binders);

 private:
  friend class ContentClientInitializer;  // To set these pointers.
  friend class InternalTestInitializer;

  // The embedder API for participating in browser logic.
  ContentBrowserClient* browser_;
  // The embedder API for participating in gpu logic.
  ContentGpuClient* gpu_;
  // The embedder API for participating in renderer logic.
  ContentRendererClient* renderer_;
  // The embedder API for participating in utility logic.
  ContentUtilityClient* utility_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_CLIENT_H_
