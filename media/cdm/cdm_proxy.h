// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_PROXY_H_
#define MEDIA_CDM_CDM_PROXY_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "media/base/cdm_context.h"
#include "media/base/media_export.h"

namespace media {

// A class that proxies part of ContentDecryptionModule (CDM) functionalities to
// a different entity, e.g. hardware CDM modules.
// In general, the interpretation of the method and callback parameters are
// protocol dependent. For enum parameters, values outside the enum range may
// not work.
class MEDIA_EXPORT CdmProxy {
 public:
  // Client of the proxy.
  class MEDIA_EXPORT Client {
   public:
    Client();
    virtual ~Client();
    // Called when there is a hardware reset and all the hardware context is
    // lost.
    virtual void NotifyHardwareReset() = 0;
  };

  enum class Status {
    kOk,
    kFail,
    kMaxValue = kFail,
  };

  enum class Protocol {
    // No supported protocol. Used in failure cases.
    kNone,
    // Method using Intel CSME.
    kIntel,
    // There will be more values in the future e.g. kD3D11RsaHardware,
    // kD3D11RsaSoftware to use the D3D11 RSA method.
    kMaxValue = kIntel,
  };

  enum class Function {
    // For Intel CSME path to call
    // ID3D11VideoContext::NegotiateCryptoSessionKeyExchange.
    kIntelNegotiateCryptoSessionKeyExchange,
    // There will be more values in the future e.g. for D3D11 RSA method.
    kMaxValue = kIntelNegotiateCryptoSessionKeyExchange,
  };

  enum class KeyType {
    kDecryptOnly,
    kDecryptAndDecode,
    kMaxValue = kDecryptAndDecode,
  };

  CdmProxy();
  virtual ~CdmProxy();

  // Returns a weak pointer of the CdmContext associated with |this|.
  // The weak pointer will be null if |this| is destroyed.
  virtual base::WeakPtr<CdmContext> GetCdmContext() = 0;

  // Callback for Initialize(). If the proxy created a crypto session, then the
  // ID for the crypto session is |crypto_session_id|.
  using InitializeCB = base::OnceCallback<
      void(Status status, Protocol protocol, uint32_t crypto_session_id)>;

  // Initializes the proxy. The status and the return values of the call is
  // reported to |init_cb|.
  virtual void Initialize(Client* client, InitializeCB init_cb) = 0;

  // Callback for Process(). |output_data| is the output of processing.
  using ProcessCB =
      base::OnceCallback<void(Status status,
                              const std::vector<uint8_t>& output_data)>;

  // Processes and updates the state of the proxy.
  // |expected_output_size| is the size of the output data passed to the
  // callback. Whether this value is required or not is protocol dependent.
  // The status and the return values of the call is reported to |process_cb|.
  virtual void Process(Function function,
                       uint32_t crypto_session_id,
                       const std::vector<uint8_t>& input_data,
                       uint32_t expected_output_data_size,
                       ProcessCB process_cb) = 0;

  // Callback for CreateMediaCryptoSession().
  // On success:
  // |crypto_session_id| is the ID for the created crypto session.
  // |output_data| is extra value, if any.
  using CreateMediaCryptoSessionCB = base::OnceCallback<
      void(Status status, uint32_t crypto_session_id, uint64_t output_data)>;

  // Creates a crypto session for handling media.
  // If extra data has to be passed to further setup the media crypto session,
  // pass the data as |input_data|.
  // The status and the return values of the call is reported to
  // |create_media_crypto_session_cb|.
  virtual void CreateMediaCryptoSession(
      const std::vector<uint8_t>& input_data,
      CreateMediaCryptoSessionCB create_media_crypto_session_cb) = 0;

  // Callback for SetKey().
  using SetKeyCB = base::OnceCallback<void(Status status)>;

  // Sets a key in the proxy.
  // |crypto_session_id| is the crypto session for decryption.
  // |key_id| is the ID of the key.
  // |key_type| is the type of the key.
  // |key_blob| is the opaque key blob for decrypting or decoding.
  // The status of the call is reported to |set_key_cb|.
  virtual void SetKey(uint32_t crypto_session_id,
                      const std::vector<uint8_t>& key_id,
                      KeyType key_type,
                      const std::vector<uint8_t>& key_blob,
                      SetKeyCB set_key_cb) = 0;

  // Callback for RemoveKey().
  using RemoveKeyCB = base::OnceCallback<void(Status status)>;

  // Removes a key from the proxy.
  // |crypto_session_id| is the crypto session for decryption.
  // |key_id| is the ID of the key.
  // The status of the call is reported to |remove_key_cb|.
  virtual void RemoveKey(uint32_t crypto_session_id,
                         const std::vector<uint8_t>& key_id,
                         RemoveKeyCB remove_key_cb) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CdmProxy);
};

using CdmProxyFactoryCB = base::RepeatingCallback<std::unique_ptr<CdmProxy>(
    const std::string& cdm_guid)>;

}  // namespace media

#endif  // MEDIA_CDM_CDM_PROXY_H_
