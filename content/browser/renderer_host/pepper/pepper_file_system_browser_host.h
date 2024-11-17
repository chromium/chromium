// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FILE_SYSTEM_BROWSER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FILE_SYSTEM_BROWSER_HOST_H_

#include <stdint.h>

#include <queue>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/renderer_host/pepper/quota_reservation.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/private/ppb_isolated_file_system_private.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/shared_impl/file_growth.h"
#include "storage/browser/file_system/file_system_context.h"
#include "url/gurl.h"

namespace content {

class BrowserPpapiHost;
class PepperFileIOHost;

class CONTENT_EXPORT PepperFileSystemBrowserHost final
    : public ppapi::host::ResourceHost {
 public:
  // Creates a new PepperFileSystemBrowserHost for a file system of a given
  // |type|. The host must be opened before use.
  PepperFileSystemBrowserHost(BrowserPpapiHost* host,
                              PP_Instance instance,
                              PP_Resource resource,
                              PP_FileSystemType type);

  PepperFileSystemBrowserHost(const PepperFileSystemBrowserHost&) = delete;
  PepperFileSystemBrowserHost& operator=(const PepperFileSystemBrowserHost&) =
      delete;

  ~PepperFileSystemBrowserHost() override;

  // Opens the PepperFileSystemBrowserHost to use an existing file system at the
  // given |root_url|. The file system at |root_url| must already be opened and
  // have the type given by GetType().
  // Calls |callback| when complete.
  void OpenExisting(const GURL& root_url, base::OnceClosure callback);

  // ppapi::host::ResourceHost overrides.
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;
  bool IsFileSystemHost() override;

  // Supports FileRefs direct access on the host side.
  PP_FileSystemType GetType() const { return type_; }
  bool IsOpened() const;
  GURL GetRootUrl() const;

  // Returns a callback which can be run on the IO thread to return a
  // FileSystemOperationRunner  Supports FileIOs direct access on the host side.
  // Non-NULL only for PP_FILESYSTEMTYPE_LOCAL{PERSISTENT,TEMPORARY}.
  using GetOperationRunnerCallback =
      base::RepeatingCallback<storage::FileSystemOperationRunner*()>;
  GetOperationRunnerCallback GetFileSystemOperationRunner() const;
  bool ChecksQuota() const { return !!quota_reservation_; }
  // Opens a file for writing with quota checks. Returns the file size in the
  // callback.
  using OpenQuotaFileCallback = base::OnceCallback<void(int64_t)>;
  void OpenQuotaFile(PepperFileIOHost* file_io_host,
                     const storage::FileSystemURL& url,
                     OpenQuotaFileCallback callback);
  // Closes the file. This must be called after OpenQuotaFile and before the
  // PepperFileIOHost is destroyed.
  void CloseQuotaFile(PepperFileIOHost* file_io_host,
                      const ppapi::FileGrowth& file_growth);

  static scoped_refptr<storage::FileSystemContext>
  GetFileSystemContextFromRenderId(int render_process_id);

  base::WeakPtr<PepperFileSystemBrowserHost> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // All interactions with FileSystemContext must occur on the IO thread as
  // it lives there.
  class IOThreadState : public base::RefCountedThreadSafe<
                            IOThreadState,
                            content::BrowserThread::DeleteOnIOThread> {
   public:
    IOThreadState(PP_FileSystemType type,
                  base::WeakPtr<PepperFileSystemBrowserHost> host);

    // |callback| needs to run on |task_runner_|.
    void OpenExistingFileSystem(
        const GURL& root_url,
        base::OnceClosure callback,
        scoped_refptr<storage::FileSystemContext> file_system_context);
    void OpenFileSystem(
        const GURL& origin,
        ppapi::host::ReplyMessageContext reply_context,
        storage::FileSystemType file_system_type,
        scoped_refptr<storage::FileSystemContext> file_system_context);
    void OpenIsolatedFileSystem(
        const GURL& origin,
        const GURL& root_url,
        const std::string& plugin_id,
        ppapi::host::ReplyMessageContext reply_context,
        const std::string& fsid,
        PP_IsolatedFileSystemType_Private type,
        scoped_refptr<storage::FileSystemContext> file_system_context);

    bool opened() const { return opened_; }
    GURL root_url() const { return root_url_; }

    scoped_refptr<storage::FileSystemContext> file_system_context() const {
      return file_system_context_;
    }

    // Supports FileIOs direct access on the host side.
    // Non-NULL only for PP_FILESYSTEMTYPE_LOCAL{PERSISTENT,TEMPORARY}.
    storage::FileSystemOperationRunner* GetFileSystemOperationRunner() const {
      return file_system_operation_runner_.get();
    }

   private:
    friend struct content::BrowserThread::DeleteOnThread<
        content::BrowserThread::IO>;
    friend class base::DeleteHelper<IOThreadState>;

    ~IOThreadState();

    void OpenFileSystemComplete(ppapi::host::ReplyMessageContext reply_context,
                                const storage::FileSystemURL& root,
                                const std::string& name,
                                base::File::Error error);

    // Runs on |task_runner_.
    void RunCallbackIfHostAlive(base::OnceClosure callback);

    void SendReplyForFileSystemIfHostAlive(
        ppapi::host::ReplyMessageContext reply_context,
        int32_t pp_error);

    void SendReplyForIsolatedFileSystem(
        ppapi::host::ReplyMessageContext reply_context,
        const std::string& fsid,
        int32_t error);

    void SetFileSystemContext(
        scoped_refptr<storage::FileSystemContext> file_system_context);

    void ShouldCreateQuotaReservation(
        base::OnceCallback<void(bool)> callback) const;
    void CreateQuotaReservation(base::OnceClosure callback);
    void GotQuotaReservation(base::OnceClosure callback,
                             scoped_refptr<QuotaReservation> quota_reservation);

    // Members below can be accessed on UI and IO threads. If accessed on the UI
    // thread, this must be done if |host_->called_open_| is true.
    bool opened_ = false;  // Whether open succeeded.
    GURL root_url_;

    // Members below should only be used on the IO thread.
    PP_FileSystemType type_;
    scoped_refptr<storage::FileSystemContext> file_system_context_;

    std::unique_ptr<storage::FileSystemOperationRunner>
        file_system_operation_runner_;

    // Used only for file systems with quota.
    // When a PepperFileIOHost calls OpenQuotaFile, we add the id and a
    // non-owning pointer to this map. CloseQuotaFile must be called when before
    // the host is destroyed.
    typedef std::map<int32_t, raw_ptr<PepperFileIOHost, CtnExperimental>>
        FileMap;
    FileMap files_;

    // Thread that this object is constructed on.
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

    // Only used on |task_runner_|.
    base::WeakPtr<PepperFileSystemBrowserHost> host_;
  };

  friend class PepperFileSystemBrowserHostTest;

  int32_t OnHostMsgOpen(ppapi::host::HostMessageContext* context,
                        int64_t expected_size);
  int32_t OnHostMsgInitIsolatedFileSystem(
      ppapi::host::HostMessageContext* context,
      const std::string& fsid,
      PP_IsolatedFileSystemType_Private type);
  int32_t OnHostMsgReserveQuota(ppapi::host::HostMessageContext* context,
                                int64_t amount,
                                const ppapi::FileGrowthMap& file_growths);

  void GotQuotaReservation(base::OnceClosure callback,
                           scoped_refptr<QuotaReservation> quota_reservation);

  void GotReservedQuota(ppapi::host::ReplyMessageContext reply_context,
                        int64_t amount,
                        const ppapi::FileSizeMap& file_sizes);

  std::string GetPluginMimeType() const;

  // Returns plugin ID generated from plugin's MIME type.
  std::string GeneratePluginId(const std::string& mime_type) const;

  static storage::FileSystemOperationRunner*
  GetFileSystemOperationRunnerInternal(
      scoped_refptr<IOThreadState> io_thread_state);

  raw_ptr<BrowserPpapiHost> browser_ppapi_host_;

  PP_FileSystemType type_;
  bool called_open_;  // whether open has been called.

  // Used only for file systems with quota.
  // When a PepperFileIOHost calls OpenQuotaFile, we add the id and a non-owning
  // pointer to this map. CloseQuotaFile must be called when before the host is
  // destroyed.
  typedef std::map<int32_t, raw_ptr<PepperFileIOHost, CtnExperimental>> FileMap;
  FileMap files_;
  int64_t reserved_quota_;
  bool reserving_quota_;
  scoped_refptr<QuotaReservation> quota_reservation_;

  std::string fsid_;  // used only for isolated filesystems.

  scoped_refptr<IOThreadState> io_thread_state_;

  base::WeakPtrFactory<PepperFileSystemBrowserHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FILE_SYSTEM_BROWSER_HOST_H_
