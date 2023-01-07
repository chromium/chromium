// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_PROTECTION_MANAGER_H_
#define MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_PROTECTION_MANAGER_H_

#include <mfapi.h>
#include <mfidl.h>
#include <windows.media.protection.h>
#include <wrl.h>

#include "base/cancelable_callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/waiting.h"
#include "media/base/win/media_foundation_cdm_proxy.h"

namespace media {

// Implements IMFContentProtectionManager
// (https://docs.microsoft.com/en-us/windows/win32/api/mfidl/nn-mfidl-imfcontentprotectionmanager)
// and ABI::Windows::Media::Protection::IMediaProtectionManager
// (https://docs.microsoft.com/en-us/uwp/api/windows.media.protection.mediaprotectionmanager)
// required by IMFMediaEngineProtectedContent::SetContentProtectionManager in
// https://docs.microsoft.com/en-us/windows/win32/api/mfmediaengine/nf-mfmediaengine-imfmediaengineprotectedcontent-setcontentprotectionmanager.
//
class MediaFoundationProtectionManager
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::RuntimeClassType::WinRtClassicComMix |
              Microsoft::WRL::RuntimeClassType::InhibitRoOriginateError>,
          IMFContentProtectionManager,
          ABI::Windows::Media::Protection::IMediaProtectionManager> {
 public:
  MediaFoundationProtectionManager();
  ~MediaFoundationProtectionManager() override;

  HRESULT RuntimeClassInitialize(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      WaitingCB waiting_cb);
  HRESULT SetCdmProxy(scoped_refptr<MediaFoundationCdmProxy> cdm_proxy);

  // IMFContentProtectionManager.
  IFACEMETHODIMP BeginEnableContent(IMFActivate* enabler_activate,
                                    IMFTopology* topology,
                                    IMFAsyncCallback* callback,
                                    IUnknown* state) override;
  IFACEMETHODIMP EndEnableContent(IMFAsyncResult* async_result) override;

  // IMediaProtectionManager.
  // MFMediaEngine can query this interface to invoke get_Properties().
  IFACEMETHODIMP add_ServiceRequested(
      ABI::Windows::Media::Protection::IServiceRequestedEventHandler* handler,
      EventRegistrationToken* cookie) override;
  IFACEMETHODIMP remove_ServiceRequested(
      EventRegistrationToken cookie) override;
  IFACEMETHODIMP add_RebootNeeded(
      ABI::Windows::Media::Protection::IRebootNeededEventHandler* handler,
      EventRegistrationToken* cookie) override;
  IFACEMETHODIMP remove_RebootNeeded(EventRegistrationToken cookie) override;
  IFACEMETHODIMP add_ComponentLoadFailed(
      ABI::Windows::Media::Protection::IComponentLoadFailedEventHandler*
          handler,
      EventRegistrationToken* cookie) override;
  IFACEMETHODIMP remove_ComponentLoadFailed(
      EventRegistrationToken cookie) override;
  IFACEMETHODIMP get_Properties(
      ABI::Windows::Foundation::Collections::IPropertySet** value) override;

 private:
  HRESULT SetPMPServer(
      ABI::Windows::Media::Protection::IMediaProtectionPMPServer* pmp_server);

  // These methods are all running on `task_runner_` due to the threading
  // requirement of `base::CancelableOnceClosure`.
  void OnBeginEnableContent();
  void OnEndEnableContent();
  void OnWaitingForKeyTimeOut();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  WaitingCB waiting_cb_;
  base::CancelableOnceClosure waiting_for_key_time_out_cb_;

  scoped_refptr<MediaFoundationCdmProxy> cdm_proxy_;
  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IPropertySet>
      property_set_;

  // This must be the last member.
  base::WeakPtrFactory<MediaFoundationProtectionManager> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_PROTECTION_MANAGER_H_
