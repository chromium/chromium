// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/win/media_foundation_protection_manager.h"

#include <mferror.h>
#include <windows.foundation.h>

#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "base/win/windows_types.h"
#include "media/base/win/hresults.h"
#include "media/base/win/mf_helpers.h"

namespace media {

using Microsoft::WRL::ComPtr;

MediaFoundationProtectionManager::MediaFoundationProtectionManager() {
  DVLOG_FUNC(1);
}

MediaFoundationProtectionManager::~MediaFoundationProtectionManager() {
  DVLOG_FUNC(1);
}

HRESULT MediaFoundationProtectionManager::RuntimeClassInitialize(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    WaitingCB waiting_cb) {
  DVLOG_FUNC(1);

  task_runner_ = std::move(task_runner);
  waiting_cb_ = std::move(waiting_cb);

  // Init an empty |property_set_| as MFMediaEngine could access it via
  // |get_Properties| before we populate it within SetPMPServer.
  base::win::ScopedHString property_set_id = base::win::ScopedHString::Create(
      RuntimeClass_Windows_Foundation_Collections_PropertySet);
  RETURN_IF_FAILED(
      base::win::RoActivateInstance(property_set_id.get(), &property_set_));
  return S_OK;
}

HRESULT MediaFoundationProtectionManager::SetCdmProxy(
    scoped_refptr<MediaFoundationCdmProxy> cdm_proxy) {
  DVLOG_FUNC(1);

  DCHECK(cdm_proxy);
  cdm_proxy_ = std::move(cdm_proxy);
  ComPtr<ABI::Windows::Media::Protection::IMediaProtectionPMPServer> pmp_server;
  RETURN_IF_FAILED(cdm_proxy_->GetPMPServer(IID_PPV_ARGS(&pmp_server)));
  RETURN_IF_FAILED(SetPMPServer(pmp_server.Get()));
  return S_OK;
}

HRESULT MediaFoundationProtectionManager::SetPMPServer(
    ABI::Windows::Media::Protection::IMediaProtectionPMPServer* pmp_server) {
  DVLOG_FUNC(1);

  DCHECK(pmp_server);
  ComPtr<ABI::Windows::Foundation::Collections::IMap<HSTRING, IInspectable*>>
      property_map;
  RETURN_IF_FAILED(property_set_.As(&property_map));

  // MFMediaEngine uses |pmp_server_key| to get the Protected Media Path (PMP)
  // server used for playing protected content. This is not currently documented
  // in MSDN.
  boolean replaced = false;
  base::win::ScopedHString pmp_server_key = base::win::ScopedHString::Create(
      L"Windows.Media.Protection.MediaProtectionPMPServer");
  RETURN_IF_FAILED(
      property_map->Insert(pmp_server_key.get(), pmp_server, &replaced));
  return S_OK;
}

HRESULT MediaFoundationProtectionManager::BeginEnableContent(
    IMFActivate* enabler_activate,
    IMFTopology* topology,
    IMFAsyncCallback* callback,
    IUnknown* state) {
  DVLOG_FUNC(1);

  ComPtr<IUnknown> unknown_object;
  ComPtr<IMFAsyncResult> async_result;
  RETURN_IF_FAILED(
      MFCreateAsyncResult(nullptr, callback, state, &async_result));
  RETURN_IF_FAILED(
      enabler_activate->ActivateObject(IID_PPV_ARGS(&unknown_object)));

  // |enabler_type| can be obtained from IMFContentEnabler
  // (https://docs.microsoft.com/en-us/windows/win32/api/mfidl/nn-mfidl-imfcontentenabler).
  // If not, try IMediaProtectionServiceRequest
  // (https://docs.microsoft.com/en-us/uwp/api/windows.media.protection.imediaprotectionservicerequest).
  GUID enabler_type = GUID_NULL;
  ComPtr<IMFContentEnabler> content_enabler;
  if (SUCCEEDED(unknown_object.As(&content_enabler))) {
    RETURN_IF_FAILED(content_enabler->GetEnableType(&enabler_type));
  } else {
    ComPtr<ABI::Windows::Media::Protection::IMediaProtectionServiceRequest>
        service_request;
    RETURN_IF_FAILED(unknown_object.As(&service_request));
    RETURN_IF_FAILED(service_request->get_Type(&enabler_type));
  }

  if (enabler_type == MFENABLETYPE_MF_RebootRequired) {
    DLOG(ERROR) << __func__ << ": MF_E_REBOOT_REQUIRED";
    return MF_E_REBOOT_REQUIRED;
  } else if (enabler_type == MFENABLETYPE_MF_UpdateRevocationInformation) {
    DLOG(ERROR) << __func__ << ": MF_E_GRL_VERSION_TOO_LOW";
    return MF_E_GRL_VERSION_TOO_LOW;
  } else if (enabler_type == MFENABLETYPE_MF_UpdateUntrustedComponent) {
    auto hr = HRESULT_FROM_WIN32(ERROR_INVALID_IMAGE_HASH);
    DLOG(ERROR) << __func__ << ": hr=" << hr;
    return hr;
  } else {
    RETURN_IF_FAILED(cdm_proxy_->ProcessContentEnabler(unknown_object.Get(),
                                                       async_result.Get()));

    // Force post task so `OnBeginEnableContent()` and `OnEndEnableContent()`
    // are always in sequence.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaFoundationProtectionManager::OnBeginEnableContent,
                       weak_factory_.GetWeakPtr()));
  }
  return S_OK;
}

HRESULT MediaFoundationProtectionManager::EndEnableContent(
    IMFAsyncResult* async_result) {
  DVLOG_FUNC(1);

  // Force post task so `OnBeginEnableContent()` and `OnEndEnableContent()` are
  // always in sequence.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaFoundationProtectionManager::OnEndEnableContent,
                     weak_factory_.GetWeakPtr()));

  // Get status from the given |async_result| for the purpose of logging.
  // Returns S_OK as there is no additional work being done here.
  HRESULT async_status = async_result->GetStatus();
  if (FAILED(async_status)) {
    DLOG(ERROR) << "Content enabling failed. hr=" << async_status;
  } else {
    DVLOG(2) << "Content enabling succeeded";
  }
  return S_OK;
}

// IMediaProtectionManager implementation
HRESULT MediaFoundationProtectionManager::add_ServiceRequested(
    ABI::Windows::Media::Protection::IServiceRequestedEventHandler* handler,
    EventRegistrationToken* cookie) {
  return E_NOTIMPL;
}

HRESULT MediaFoundationProtectionManager::remove_ServiceRequested(
    EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

HRESULT MediaFoundationProtectionManager::add_RebootNeeded(
    ABI::Windows::Media::Protection::IRebootNeededEventHandler* handler,
    EventRegistrationToken* cookie) {
  return E_NOTIMPL;
}

HRESULT MediaFoundationProtectionManager::remove_RebootNeeded(
    EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

HRESULT MediaFoundationProtectionManager::add_ComponentLoadFailed(
    ABI::Windows::Media::Protection::IComponentLoadFailedEventHandler* handler,
    EventRegistrationToken* cookie) {
  return E_NOTIMPL;
}

HRESULT MediaFoundationProtectionManager::remove_ComponentLoadFailed(
    EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

HRESULT MediaFoundationProtectionManager::get_Properties(
    ABI::Windows::Foundation::Collections::IPropertySet** properties) {
  DVLOG_FUNC(2);
  if (!properties)
    return E_POINTER;
  return property_set_.CopyTo(properties);
}

void MediaFoundationProtectionManager::OnBeginEnableContent() {
  DVLOG_FUNC(2);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // If EnableContent takes too long, report waiting for key status. Choose a
  // timeout of 500ms to be on the safe side, e.g. on slower machines.
  const auto kWaitingForKeyTimeOut = base::Milliseconds(500);

  waiting_for_key_time_out_cb_.Reset(
      base::BindOnce(&MediaFoundationProtectionManager::OnWaitingForKeyTimeOut,
                     weak_factory_.GetWeakPtr()));
  task_runner_->PostDelayedTask(FROM_HERE,
                                waiting_for_key_time_out_cb_.callback(),
                                kWaitingForKeyTimeOut);
}

void MediaFoundationProtectionManager::OnEndEnableContent() {
  DVLOG_FUNC(2);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  waiting_for_key_time_out_cb_.Cancel();
}

void MediaFoundationProtectionManager::OnWaitingForKeyTimeOut() {
  DVLOG_FUNC(2);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  waiting_for_key_time_out_cb_.Cancel();
  waiting_cb_.Run(WaitingReason::kNoDecryptionKey);
}

}  // namespace media
