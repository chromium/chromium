// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ios/nfc_host.h"

#include <CoreNFC/CoreNFC.h>

#include <utility>

#include "base/apple/foundation_util.h"
#include "base/strings/string_view_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/web_contents.h"
#include "services/device/public/cpp/nfc/nfc_utils.h"
#include "services/device/public/mojom/nfc.mojom.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace {
device::mojom::NDEFRawMessagePtr ConvertNFCDefRawMessage(
    NFCNDEFMessage* message) {
  if (!message) {
    return nullptr;
  }
  std::vector<device::mojom::NDEFRawRecordPtr> records;
  for (NFCNDEFPayload* record : [message records]) {
    device::mojom::NDEFRawRecordPtr mojo_record =
        device::mojom::NDEFRawRecord::New();
    mojo_record->type_name_format =
        device::MapCoreNFCFormat([record typeNameFormat]);
    auto identifier_span = base::apple::NSDataToSpan([record identifier]);
    mojo_record->identifier.assign(identifier_span.begin(),
                                   identifier_span.end());
    auto payload_span = base::apple::NSDataToSpan([record payload]);
    mojo_record->payload.assign(payload_span.begin(), payload_span.end());
    auto type_span = base::apple::NSDataToSpan([record type]);
    mojo_record->type.assign(type_span.begin(), type_span.end());
    records.push_back(std::move(mojo_record));
  }
  return device::mojom::NDEFRawMessage::New(std::move(records));
}

NSData* NSDataFromOptionalString(const std::optional<std::string>& value) {
  if (!value) {
    return [NSData data];
  }
  // value will already be utf-8 encoded.
  return [NSData dataWithBytes:value->data() length:value->size()];
}

NSData* NSDataFromSpan(const std::vector<uint8_t>& value) {
  return [NSData dataWithBytes:value.data() length:value.size()];
}

NFCNDEFPayload* MojoNDEFRecordToCoreNFC(device::mojom::NDEFRecordPtr& record) {
  if (record->category ==
      device::mojom::NDEFRecordTypeCategory::kStandardized) {
    if (record->record_type == "url" || record->record_type == "absolute-url") {
      return [NFCNDEFPayload
          wellKnownTypeURIPayloadWithString:base::SysUTF8ToNSString(
                                                base::as_string_view(
                                                    record->data))];
    } else if (record->record_type == "text") {
      NSLocale* locale = nullptr;
      if (record->lang) {
        locale = [NSLocale
            localeWithLocaleIdentifier:base::SysUTF8ToNSString(*record->lang)];
      }
      // TODO(crbug.com/420902570): CoreNFC will encode this message in
      // UTF-16 even though it doesn't have to. And it will put a BOM
      // marker there as well.
      return [NFCNDEFPayload
          wellKnownTypeTextPayloadWithString:base::SysUTF8ToNSString(
                                                 base::as_string_view(
                                                     record->data))
                                      locale:locale];
    } else if (record->record_type == "mime") {
      return [[NFCNDEFPayload alloc]
          initWithFormat:NFCTypeNameFormatMedia
                    type:NSDataFromOptionalString(record->media_type)
              identifier:NSDataFromOptionalString(record->id)
                 payload:NSDataFromSpan(record->data)];
    } else if (record->record_type == "empty") {
      return [[NFCNDEFPayload alloc] initWithFormat:NFCTypeNameFormatEmpty
                                               type:[NSData data]
                                         identifier:[NSData data]
                                            payload:[NSData data]];
    } else if (record->record_type == "smart-poster") {
    }
    // TODO(crbug.com/420902570): CoreNFC doesn't support encoding nested
    // records yet.
    return nullptr;
  } else if (record->category ==
             device::mojom::NDEFRecordTypeCategory::kExternal) {
    // TODO(crbug.com/420902570): CoreNFC doesn't support encoding nested
    // records yet.
    return nullptr;
  } else if (record->category ==
             device::mojom::NDEFRecordTypeCategory::kLocal) {
    // TODO(crbug.com/420902570): CoreNFC doesn't support encoding nested
    // records yet.
    return nullptr;
  }
  return nullptr;
}

}  // namespace

@interface NFCSessionImpl : NSObject <NFCNDEFReaderSessionDelegate> {
  base::WeakPtr<content::NFCHost> _host;
  scoped_refptr<base::SequencedTaskRunner> _mainTaskRunner;
  NFCNDEFReaderSession* _session;
  id<NFCNDEFTag> _tag;
}

- (instancetype)initWithHost:(base::WeakPtr<content::NFCHost>)host
                  withRunner:(scoped_refptr<base::SequencedTaskRunner>)runner;

@end

@implementation NFCSessionImpl

- (instancetype)initWithHost:(base::WeakPtr<content::NFCHost>)host
                  withRunner:(scoped_refptr<base::SequencedTaskRunner>)runner {
  self = [self init];
  if (self) {
    _host = host;
    _mainTaskRunner = runner;
    _session = [[NFCNDEFReaderSession alloc] initWithDelegate:self
                                                        queue:nil
                                     invalidateAfterFirstRead:false];
    [_session beginSession];
  }
  return self;
}

- (void)writeTag:(device::mojom::NDEFMessagePtr)message {
  auto writeTagCallback = base::BindPostTask(
      _mainTaskRunner,
      base::BindRepeating(&content::NFCHost::TagWriteComplete, _host));
  void (^writeTagComplete)(NSError* error) = ^void(NSError* error) {
    self->_session.alertMessage = @"Card written";
    writeTagCallback.Run(error);
  };

  _session.alertMessage = @"Writing card";

  bool error = false;
  NSMutableArray<NFCNDEFPayload*>* payloads = [NSMutableArray new];
  for (device::mojom::NDEFRecordPtr& record : message->data) {
    NFCNDEFPayload* ndefRecord = MojoNDEFRecordToCoreNFC(record);
    if (!ndefRecord) {
      error = true;
      break;
    }
    [payloads addObject:ndefRecord];
  }

  if (error) {
    // Indicate an error.
    writeTagCallback.Run(true);
    return;
  }
  NFCNDEFMessage* ndefMessage =
      [[NFCNDEFMessage alloc] initWithNDEFRecords:payloads];
  [_tag writeNDEF:ndefMessage completionHandler:writeTagComplete];
}

- (void)makeReadOnly {
  auto writeTagCallback = base::BindPostTask(
      _mainTaskRunner,
      base::BindRepeating(&content::NFCHost::TagWriteLockComplete, _host));
  void (^writeTagComplete)(NSError* error) = ^void(NSError* error) {
    self->_session.alertMessage = @"Card marked readonly";
    writeTagCallback.Run(error);
  };

  _session.alertMessage = @"Writing card";
  [_tag writeLockWithCompletionHandler:writeTagComplete];

  // TODO(dtapuska): Implement me.
}

- (void)readTag {
  auto readTagCallback = base::BindPostTask(
      _mainTaskRunner,
      base::BindRepeating(&content::NFCHost::TagReadComplete, _host));
  void (^readTagComplete)(NFCNDEFMessage* message, NSError* error) =
      ^void(NFCNDEFMessage* message, NSError* error) {
        self->_session.alertMessage = @"Card read";
        device::mojom::NDEFRawMessagePtr mojoMessage;
        if (!error) {
          mojoMessage = ConvertNFCDefRawMessage(message);
        }
        readTagCallback.Run(std::move(mojoMessage), error);
      };
  [_tag readNDEFWithCompletionHandler:readTagComplete];
}

- (void)readerSession:(NFCNDEFReaderSession*)session
        didDetectTags:(NSArray<__kindof id<NFCNDEFTag>>*)tags {
  if ([tags count] > 1) {
    // WebNFC only allows reading one tag in the field at a time.
    return;
  }

  _session.alertMessage = @"Card connected";
  _tag = tags[0];
  auto queryTagCallback = base::BindPostTask(
      _mainTaskRunner,
      base::BindRepeating(&content::NFCHost::TagQueried, _host));

  void (^queryNdefComplete)(NFCNDEFStatus status, NSUInteger capacity,
                            NSError* error) =
      ^void(NFCNDEFStatus status, NSUInteger capacity, NSError* error) {
        self->_session.alertMessage = @"Reading card";
        content::NFCHost::TagStatus tagStatus;
        switch (status) {
          case NFCNDEFStatusNotSupported:
            tagStatus = content::NFCHost::TagStatus::kNotSupported;
            break;
          case NFCNDEFStatusReadOnly:
            tagStatus = content::NFCHost::TagStatus::kReadOnly;
            break;
          case NFCNDEFStatusReadWrite:
            tagStatus = content::NFCHost::TagStatus::kReadWrite;
            break;
        }
        queryTagCallback.Run(tagStatus, error);
      };

  void (^connectTagComplete)(NSError* error) = ^void(NSError* error) {
    self->_session.alertMessage = @"Card detected";
    [self->_tag queryNDEFStatusWithCompletionHandler:queryNdefComplete];
  };

  [_session connectToTag:tags[0] completionHandler:connectTagComplete];
}

- (void)readerSession:(NFCNDEFReaderSession*)session
    didInvalidateWithError:(NSError*)error {
  _mainTaskRunner->PostTask(
      FROM_HERE,
      base::BindOnce(&content::NFCHost::ReaderInvalidated, _host, error));
}

- (void)readerSession:(NFCNDEFReaderSession*)session
       didDetectNDEFs:(NSArray<NFCNDEFMessage*>*)messages {
  // We should not see this API called according to CoreNFC since we implement
  // didDetectTags.
  NOTREACHED();
}

@end

namespace content {

namespace {

const char* ErrorToString(device::mojom::NDEFErrorType error) {
  switch (error) {
    case device::mojom::NDEFErrorType::NOT_ALLOWED:
      return "Not allowed";
    case device::mojom::NDEFErrorType::NOT_SUPPORTED:
      return "Not supported";
    case device::mojom::NDEFErrorType::NOT_READABLE:
      return "Not readable";
    case device::mojom::NDEFErrorType::INVALID_MESSAGE:
      return "Invalid message";
    case device::mojom::NDEFErrorType::OPERATION_CANCELLED:
      return "Operation cancelled";
    case device::mojom::NDEFErrorType::IO_ERROR:
      return "IO error";
  }
}

}  // namespace

class NFCSessionHolder {
 public:
  NFCSessionImpl* __strong session_;
};

NFCHost::PendingPush::PendingPush(device::mojom::NDEFMessagePtr message_a,
                                  device::mojom::NDEFWriteOptionsPtr options_a,
                                  device::mojom::NFC::PushCallback callback_a)
    : message(std::move(message_a)),
      options(std::move(options_a)),
      callback(std::move(callback_a)) {}
NFCHost::PendingPush::~PendingPush() = default;

NFCHost::NFCHost(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  CHECK(web_contents);

  permission_controller_ =
      web_contents->GetBrowserContext()->GetPermissionController();
}

NFCHost::~NFCHost() {
  Close();
}

void NFCHost::GetNFC(RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<device::mojom::NFC> receiver) {
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from the outermost frame and restrict
  // from the prerendered page. Well-behaved renderer can't trigger this method
  // since mojo capabiliy control blocks during prerendering and permission
  // request of WebNFC from fenced frames is denied.
  if (render_frame_host->GetParent()) {
    mojo::ReportBadMessage("WebNFC is not allowed in an iframe.");
    return;
  }
  if (render_frame_host->GetLifecycleState() ==
      RenderFrameHost::LifecycleState::kPrerendering) {
    mojo::ReportBadMessage("WebNFC is not allowed in a prerendered page.");
    return;
  }
  if (render_frame_host->IsNestedWithinFencedFrame()) {
    mojo::ReportBadMessage("WebNFC is not allowed within in a fenced frame.");
    return;
  }
  if (web_contents()->GetPrimaryMainFrame() != render_frame_host) {
    mojo::ReportBadMessage("WebNFC not on primary main frame.");
    return;
  }

  if (render_frame_host->GetBrowserContext()
          ->GetPermissionController()
          ->GetPermissionStatusForCurrentDocument(
              content::PermissionDescriptorUtil::
                  CreatePermissionDescriptorForPermissionType(
                      blink::PermissionType::NFC),
              render_frame_host) != blink::mojom::PermissionStatus::GRANTED) {
    return;
  }

  if (!subscription_id_) {
    // base::Unretained() is safe here because the subscription is canceled when
    // this object is destroyed.
    subscription_id_ =
        permission_controller_->SubscribeToPermissionResultChange(
            content::PermissionDescriptorUtil::
                CreatePermissionDescriptorForPermissionType(
                    blink::PermissionType::NFC),
            /*render_process_host=*/nullptr, render_frame_host,
            render_frame_host->GetMainFrame()
                ->GetLastCommittedOrigin()
                .GetURL(),
            /*should_include_device_status=*/false,
            base::BindRepeating(&NFCHost::OnPermissionResultChange,
                                base::Unretained(this)));
  }

  // Release any old receiver and rebind to the new primary main frame.
  ClearState();
  receiver_.Bind(std::move(receiver));
}

void NFCHost::RenderFrameHostChanged(RenderFrameHost* old_host,
                                     RenderFrameHost* new_host) {
  // If the main frame has been replaced then close an old NFC connection.
  if (new_host->IsInPrimaryMainFrame()) {
    Close();
  }
}

void NFCHost::OnVisibilityChanged(Visibility visibility) {
  MaybeResumeOrSuspendOperations(visibility);
}

void NFCHost::MaybeResumeOrSuspendOperations(Visibility visibility) {
  // NFC operations should be suspended.
  // https://w3c.github.io/web-nfc/#nfc-suspended
  if (visibility == Visibility::VISIBLE) {
    EnableSessionIfNecessary();
  } else {
    CancelPush();
    CancelMakeReadOnly();
    session_.reset();
    tag_status_.reset();
  }
}

void NFCHost::OnPermissionResultChange(PermissionResult permission_result) {
  if (permission_result.status != blink::mojom::PermissionStatus::GRANTED) {
    Close();
  }
}

void NFCHost::Close() {
  permission_controller_->UnsubscribeFromPermissionResultChange(
      subscription_id_);
  subscription_id_ = PermissionController::SubscriptionId();
  ClearState();
}

void NFCHost::ClearState() {
  receiver_.reset();
  client_remote_.reset();
  pending_push_.reset();
  pending_read_only_ = device::mojom::NFC::MakeReadOnlyCallback();
  watches_.clear();
  tag_status_.reset();
  session_.reset();
}

void NFCHost::SetClient(
    mojo::PendingRemote<device::mojom::RawNFCClient> client) {
  client_remote_.Bind(std::move(client));
}

void NFCHost::Push(device::mojom::NDEFMessagePtr message,
                   device::mojom::NDEFWriteOptionsPtr options,
                   device::mojom::NFC::PushCallback callback) {
  if (suspended_) {
    std::move(callback).Run(device::mojom::NDEFError::New(
        device::mojom::NDEFErrorType::OPERATION_CANCELLED,
        ErrorToString(device::mojom::NDEFErrorType::OPERATION_CANCELLED)));
    return;
  }

  CancelPush();
  pending_push_ = std::make_unique<PendingPush>(
      std::move(message), std::move(options), std::move(callback));
  if (tag_status_.has_value()) {
    switch (tag_status_.value()) {
      case TagStatus::kReadWrite:
        HandlePendingPushOperation();
        break;
      case TagStatus::kNotSupported:
        PendingPushOperationComplete(
            device::mojom::NDEFErrorType::NOT_SUPPORTED);
        break;
      case TagStatus::kReadOnly:
        PendingPushOperationComplete(device::mojom::NDEFErrorType::NOT_ALLOWED);
        break;
    }
  }
  EnableSessionIfNecessary();
}

void NFCHost::CancelPush() {
  PendingPushOperationComplete(
      device::mojom::NDEFErrorType::OPERATION_CANCELLED);
}

void NFCHost::MakeReadOnly(device::mojom::NFC::MakeReadOnlyCallback callback) {
  // Cancel any old operation.
  CancelMakeReadOnly();
  pending_read_only_ = std::move(callback);
  if (tag_status_.has_value()) {
    switch (tag_status_.value()) {
      case TagStatus::kReadWrite:
        HandlePendingMakeReadOnlyOperation();
        break;
      case TagStatus::kNotSupported:
        PendingMakeReadOnlyOperationComplete(
            device::mojom::NDEFErrorType::NOT_SUPPORTED);
        break;
      case TagStatus::kReadOnly:
        PendingMakeReadOnlyOperationComplete(
            device::mojom::NDEFErrorType::NOT_ALLOWED);
        break;
    }
  }
  EnableSessionIfNecessary();
}

void NFCHost::CancelMakeReadOnly() {
  PendingMakeReadOnlyOperationComplete(
      device::mojom::NDEFErrorType::OPERATION_CANCELLED);
}

void NFCHost::Watch(uint32_t watch_id,
                    device::mojom::NFC::WatchCallback callback) {
  if (watches_.contains(watch_id)) {
    receiver_.ReportBadMessage("WebNFC duplicate watch ID.");
    return;
  }
  watches_.insert(watch_id);
  EnableSessionIfNecessary();
  std::move(callback).Run(nullptr);
}

void NFCHost::CancelWatch(uint32_t watch_id) {
  watches_.erase(watch_id);
  DisableSessionIfNecessary();
}

void NFCHost::EnableSessionIfNecessary() {
  if (session_) {
    return;
  }
  session_ = std::make_unique<NFCSessionHolder>();
  session_->session_ =
      [[NFCSessionImpl alloc] initWithHost:weak_ptr_factory_.GetWeakPtr()
                                withRunner:main_task_runner_];
}

void NFCHost::DisableSessionIfNecessary() {
  if (!watches_.empty() || pending_read_only_ || pending_push_ || !session_) {
    return;
  }
  tag_status_.reset();
  session_.reset();
}

void NFCHost::TagQueried(TagStatus status, bool error) {
  tag_status_ = status;
  tag_has_records_ = false;
  switch (status) {
    case TagStatus::kNotSupported:
      PendingWatchOperationComplete(
          device::mojom::NDEFErrorType::NOT_SUPPORTED);
      PendingPushOperationComplete(device::mojom::NDEFErrorType::NOT_SUPPORTED);
      PendingMakeReadOnlyOperationComplete(
          device::mojom::NDEFErrorType::NOT_SUPPORTED);
      break;
    case TagStatus::kReadOnly:
    case TagStatus::kReadWrite:
      [session_->session_ readTag];
      break;
  }
}

void NFCHost::TagReadComplete(device::mojom::NDEFRawMessagePtr message,
                              bool error) {
  if (!message || error || !client_remote_) {
    PendingWatchOperationComplete(device::mojom::NDEFErrorType::IO_ERROR);
    PendingPushOperationComplete(device::mojom::NDEFErrorType::IO_ERROR);
    PendingMakeReadOnlyOperationComplete(
        device::mojom::NDEFErrorType::IO_ERROR);
    return;
  }

  tag_has_records_ = !message->data.empty();
  if (!watches_.empty()) {
    std::vector<uint32_t> watches(watches_.begin(), watches_.end());
    client_remote_->OnWatch(watches, std::move(message));
  }

  if (tag_status_ == TagStatus::kReadOnly) {
    PendingPushOperationComplete(device::mojom::NDEFErrorType::NOT_ALLOWED);
    PendingMakeReadOnlyOperationComplete(
        device::mojom::NDEFErrorType::NOT_ALLOWED);
  } else {
    HandlePendingPushOperation();
  }
}

void NFCHost::TagWriteComplete(bool error) {
  if (error || !client_remote_ || !pending_push_) {
    PendingPushOperationComplete(device::mojom::NDEFErrorType::IO_ERROR);
    PendingMakeReadOnlyOperationComplete(
        device::mojom::NDEFErrorType::IO_ERROR);
    return;
  }
  std::move(pending_push_->callback).Run(nullptr);
  pending_push_.reset();
  HandlePendingMakeReadOnlyOperation();
}

void NFCHost::TagWriteLockComplete(bool error) {
  if (error || !client_remote_ || !pending_read_only_) {
    PendingMakeReadOnlyOperationComplete(
        device::mojom::NDEFErrorType::IO_ERROR);
    return;
  }
  std::move(pending_read_only_).Run(nullptr);
  DisableSessionIfNecessary();
}

void NFCHost::PendingWatchOperationComplete(
    device::mojom::NDEFErrorType error) {
  if (watches_.empty() || !client_remote_) {
    return;
  }
  client_remote_->OnError(
      device::mojom::NDEFError::New(error, ErrorToString(error)));
  watches_.clear();
  DisableSessionIfNecessary();
}

void NFCHost::ReaderInvalidated(bool error) {
  PendingWatchOperationComplete(device::mojom::NDEFErrorType::NOT_SUPPORTED);
  PendingPushOperationComplete(device::mojom::NDEFErrorType::NOT_SUPPORTED);
  PendingMakeReadOnlyOperationComplete(
      device::mojom::NDEFErrorType::NOT_SUPPORTED);
}

void NFCHost::HandlePendingPushOperation() {
  if (!pending_push_) {
    HandlePendingMakeReadOnlyOperation();
    return;
  }
  if (pending_push_->options && !pending_push_->options->overwrite &&
      tag_has_records_) {
    PendingPushOperationComplete(device::mojom::NDEFErrorType::NOT_ALLOWED);
    PendingMakeReadOnlyOperationComplete(
        device::mojom::NDEFErrorType::NOT_ALLOWED);
    return;
  }
  [session_->session_ writeTag:std::move(pending_push_->message)];
}

void NFCHost::PendingPushOperationComplete(device::mojom::NDEFErrorType error) {
  if (pending_push_) {
    std::move(pending_push_->callback)
        .Run(device::mojom::NDEFError::New(error, ErrorToString(error)));
    pending_push_.reset();
  }
  DisableSessionIfNecessary();
}

void NFCHost::HandlePendingMakeReadOnlyOperation() {
  if (pending_read_only_) {
    [session_->session_ makeReadOnly];
  }
}

void NFCHost::PendingMakeReadOnlyOperationComplete(
    device::mojom::NDEFErrorType error) {
  if (pending_read_only_) {
    std::move(pending_read_only_)
        .Run(device::mojom::NDEFError::New(error, ErrorToString(error)));
  }
  DisableSessionIfNecessary();
}

}  // namespace content
