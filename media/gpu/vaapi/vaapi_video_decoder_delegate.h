// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_VIDEO_DECODER_DELEGATE_H_
#define MEDIA_GPU_VAAPI_VAAPI_VIDEO_DECODER_DELEGATE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "media/base/decryptor.h"
#include "media/base/encryption_scheme.h"
#include "media/base/subsample_entry.h"
#include "third_party/libva_protected_content/va_protected_content.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace chromeos {
class ChromeOsCdmContext;
}  // namespace chromeos
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace media {

class CdmContext;
class VaapiDecodeSurfaceHandler;
class DecryptConfig;
class VaapiWrapper;
class VASurface;

// The common part of each AcceleratedVideoDecoder's Accelerator for VA-API.
// This class allows clients to reset VaapiWrapper in case of a profile change.
// VaapiDecodeSurfaceHandler must stay alive for the lifetime of this class.
// This also handles all of the shared functionality relating to protected
// sessions in VA-API.
class VaapiVideoDecoderDelegate {
 public:
  // Callback when using protected mode to indicate that if waiting, the
  // decoder should resume again. If |success| is false, then decoding should
  // fail.
  using ProtectedSessionUpdateCB = base::RepeatingCallback<void(bool success)>;

  VaapiVideoDecoderDelegate(
      VaapiDecodeSurfaceHandler* const vaapi_dec,
      scoped_refptr<VaapiWrapper> vaapi_wrapper,
      ProtectedSessionUpdateCB on_protected_session_update_cb,
      CdmContext* cdm_context,
      EncryptionScheme encryption_scheme = EncryptionScheme::kUnencrypted);
  virtual ~VaapiVideoDecoderDelegate();

  void set_vaapi_wrapper(scoped_refptr<VaapiWrapper> vaapi_wrapper);
  virtual void OnVAContextDestructionSoon();

  VaapiVideoDecoderDelegate(const VaapiVideoDecoderDelegate&) = delete;
  VaapiVideoDecoderDelegate& operator=(const VaapiVideoDecoderDelegate&) =
      delete;

  // Should be called when kTryAgain is returned from decoding to determine if
  // we should try to recover the session by sending a kDecodeStateLost message
  // up through the WaitingCB in the decoder. Returns true if we should send the
  // kDecodeStateLost message.
  bool HasInitiatedProtectedRecovery();

 protected:
  // Sets the |decrypt_config| currently active for this stream. Returns true if
  // that config is compatible with the existing one (for example, you can't
  // change encryption schemes midstream).
  bool SetDecryptConfig(std::unique_ptr<DecryptConfig> decrypt_config);

  enum class ProtectedSessionState {
    kNotCreated,
    kInProcess,
    kCreated,
    kNeedsRecovery,
    kFailed
  };

  // Ensures we have a protected session setup and attached to the active
  // |vaapi_wrapper_| we are using. We are in the corresponding state returned
  // when this call returns. |full_sample| indicates if we are using full sample
  // encryption or not and must remain consistent for a session. If everything
  // is setup for a protected session, it will fill in the |crypto_params|.
  // |segments| must retain its memory until the frame is submitted.
  // |subsamples| is for the current slice. |size| is the size of the slice
  // data. This should be called if IsEncrypted() is true even if the current
  // data is not encrypted (i.e. |subsamples| is empty).
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ProtectedSessionState SetupDecryptDecode(
      bool full_sample,
      size_t size,
      VAEncryptionParameters* crypto_params,
      std::vector<VAEncryptionSegmentInfo>* segments,
      const std::vector<SubsampleEntry>& subsamples);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Returns true if we are handling encrypted content, in which case
  // SetupDecryptDecode() should be called for every slice. This is specifically
  // for Intel platforms.
  bool IsEncryptedSession() const {
    return (encryption_scheme_ != EncryptionScheme::kUnencrypted) &&
           !transcryption_;
  }

  // Returns true if we are handling transcrypted content. This is specifically
  // for AMD platforms that normalize encrypted content with the TEE rather than
  // passing all the parameters into libva.
  bool IsTranscrypted() const { return transcryption_; }

  // Should be called by subclasses if a failure occurs during actual decoding.
  // This will check if we are using protected mode and it's in a state that
  // can be recovered which should resolve the error. If this method returns
  // true, then the caller should return kTryAgain from the accelerator to kick
  // off the rest of the recovery process.
  bool NeedsProtectedSessionRecovery();

  // Should be invoked by subclasses if they successfully decoded protected
  // video. This is so we can reset our tracker to indicate we successfully
  // recovered from protected session loss. It is fine to call this method on
  // every successful protected decode.
  void ProtectedDecodedSucceeded();

  // Returns the key_id string for the current DecryptConfig.
  std::string GetDecryptKeyId() const;

  // Both owned by caller.
  const raw_ptr<VaapiDecodeSurfaceHandler> vaapi_dec_;
  scoped_refptr<VaapiWrapper> vaapi_wrapper_;

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  void OnGetHwConfigData(bool success, const std::vector<uint8_t>& config_data);
  void OnGetHwKeyData(const std::string& key_id,
                      Decryptor::Status status,
                      const std::vector<uint8_t>& key_data);
  void RecoverProtectedSession();

  // All members below pertain to protected content playback.
  ProtectedSessionUpdateCB on_protected_session_update_cb_;
  EncryptionScheme encryption_scheme_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Not owned.
  raw_ptr<chromeos::ChromeOsCdmContext> chromeos_cdm_context_ = nullptr;
  EncryptionScheme last_used_encryption_scheme_{EncryptionScheme::kUnencrypted};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ProtectedSessionState protected_session_state_;
  std::unique_ptr<DecryptConfig> decrypt_config_;
  std::vector<uint8_t> hw_identifier_;
  std::map<std::string, std::vector<uint8_t>> hw_key_data_map_;

  // This will only be true on AMD platforms where we support encrypted content
  // and the content is encrypted.
  bool transcryption_ = false;

  // This gets set to true if we indicated we should try to recover from
  // protected session loss. We use this so that we don't go into a loop where
  // we repeatedly retry recovery over and over.
  bool performing_recovery_;

  base::WeakPtrFactory<VaapiVideoDecoderDelegate> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_VIDEO_DECODER_DELEGATE_H_
