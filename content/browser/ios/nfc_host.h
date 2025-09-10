// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IOS_NFC_HOST_H_
#define CONTENT_BROWSER_IOS_NFC_HOST_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/nfc.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-forward.h"

namespace content {

class NFCSessionHolder;

class NFCHost : public WebContentsObserver, public device::mojom::NFC {
 public:
  explicit NFCHost(WebContents* web_contents);

  NFCHost(const NFCHost&) = delete;
  NFCHost& operator=(const NFCHost&) = delete;

  ~NFCHost() override;

  void GetNFC(RenderFrameHost* render_frame_host,
              mojo::PendingReceiver<device::mojom::NFC> receiver);

  // WebContentsObserver implementation.
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void OnVisibilityChanged(Visibility visibility) override;

  // device::mojom::NFC implementation.
  void SetClient(
      mojo::PendingRemote<device::mojom::RawNFCClient> client) override;
  void Push(device::mojom::NDEFMessagePtr message,
            device::mojom::NDEFWriteOptionsPtr options,
            device::mojom::NFC::PushCallback callback) override;
  void CancelPush() override;
  void MakeReadOnly(device::mojom::NFC::MakeReadOnlyCallback callback) override;
  void CancelMakeReadOnly() override;
  void Watch(uint32_t id, device::mojom::NFC::WatchCallback callback) override;
  void CancelWatch(uint32_t id) override;

  enum class TagStatus { kReadWrite, kReadOnly, kNotSupported };
  void TagQueried(TagStatus status, bool error);
  void TagReadComplete(device::mojom::NDEFRawMessagePtr message, bool error);
  void TagWriteComplete(bool error);
  void TagWriteLockComplete(bool error);
  void ReaderInvalidated(bool error);

 private:
  void MaybeResumeOrSuspendOperations(Visibility visibility);
  void OnPermissionResultChange(PermissionResult permission_result);
  void Close();
  void ClearState();
  void EnableSessionIfNecessary();
  void DisableSessionIfNecessary();
  void PendingWatchOperationComplete(device::mojom::NDEFErrorType error);
  void HandlePendingPushOperation();
  void PendingPushOperationComplete(device::mojom::NDEFErrorType error);
  void HandlePendingMakeReadOnlyOperation();
  void PendingMakeReadOnlyOperationComplete(device::mojom::NDEFErrorType error);

  // The permission controller for this browser context.
  raw_ptr<PermissionController> permission_controller_;

  // Permission change subscription ID provided by |permission_controller_|.
  PermissionController::SubscriptionId subscription_id_;

  mojo::Receiver<device::mojom::NFC> receiver_{this};
  mojo::Remote<device::mojom::RawNFCClient> client_remote_;

  struct PendingPush {
    PendingPush(device::mojom::NDEFMessagePtr,
                device::mojom::NDEFWriteOptionsPtr,
                device::mojom::NFC::PushCallback);
    ~PendingPush();

    device::mojom::NDEFMessagePtr message;
    device::mojom::NDEFWriteOptionsPtr options;
    device::mojom::NFC::PushCallback callback;
  };
  std::unique_ptr<PendingPush> pending_push_;
  device::mojom::NFC::MakeReadOnlyCallback pending_read_only_;

  absl::flat_hash_set<uint32_t> watches_;
  std::optional<TagStatus> tag_status_;
  bool tag_has_records_ = false;
  std::unique_ptr<NFCSessionHolder> session_;
  bool suspended_ = false;

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  base::WeakPtrFactory<NFCHost> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_IOS_NFC_HOST_H_
