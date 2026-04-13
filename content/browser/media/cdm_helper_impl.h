// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_HELPER_IMPL_H_
#define CONTENT_BROWSER_MEDIA_CDM_HELPER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/cdm_helper.h"
#include "media/mojo/mojom/content_decryption_module.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media::mojom {
class CdmFactory;
}  // namespace media::mojom

namespace content {

class CdmHelperImpl : public CdmHelper {
 public:
  CdmHelperImpl();
  ~CdmHelperImpl() override;

  void Initialize(
      const std::string& server_ceritificate,
      const std::string& key_system,
      base::OnceCallback<void(InitializeResult)> init_callback) override;
  void SignChallenge(
      const std::string& challenge,
      base::OnceCallback<void(const std::string&, SignChallengeResult)>
          callback) override;

 private:
  // A simple client to get CDM events.
  class CdmClient : public media::mojom::ContentDecryptionModuleClient {
   public:
    CdmClient();
    ~CdmClient() override;

    // media::mojom::ContentDecryptionModuleClient implementation.
    void OnSessionMessage(const std::string& session_id,
                          media::CdmMessageType message_type,
                          const std::vector<uint8_t>& message) override;
    void OnSessionClosed(const std::string& session_id,
                         media::CdmSessionClosedReason reason) override;
    void OnSessionKeysChange(
        const std::string& session_id,
        bool has_additional_usable_key,
        std::vector<std::unique_ptr<media::CdmKeyInformation>> keys_info)
        override;
    void OnSessionExpirationUpdate(const std::string& session_id,
                                   double new_expiry_time_sec) override;

    void SetCallback(base::OnceCallback<void(const std::string&,
                                             SignChallengeResult)> callback);
    bool HasCallback();

   private:
    base::OnceCallback<void(const std::string&, SignChallengeResult)> callback_;
  };

  void OnCdmCreated(
      const std::string& server_ceritificate,
      base::OnceCallback<void(InitializeResult)> init_callback,
      mojo::PendingRemote<media::mojom::ContentDecryptionModule> cdm,
      media::mojom::CdmContextPtr cdm_context,
      media::CreateCdmStatus status);

  void OnSetServerCertificate(
      base::OnceCallback<void(InitializeResult)> init_callback,
      media::mojom::CdmPromiseResultPtr result);

  mojo::Remote<media::mojom::CdmFactory> cdm_factory_;
  mojo::Remote<media::mojom::ContentDecryptionModule> cdm_;
  std::unique_ptr<CdmClient> cdm_client_;
  std::unique_ptr<
      mojo::AssociatedReceiver<media::mojom::ContentDecryptionModuleClient>>
      cdm_client_receiver_;

  base::WeakPtrFactory<CdmHelperImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_HELPER_IMPL_H_
