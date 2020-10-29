// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/modules/v8/serialization/v8_script_value_serializer_for_modules.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_crypto.h"
#include "third_party/blink/public/platform/web_crypto_key.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/v8_script_value_serializer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_rect_read_only.h"
#include "third_party/blink/renderer/bindings/modules/v8/serialization/web_crypto_sub_tags.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_crypto_key.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_dom_file_system.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_directory_handle.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_file_handle.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_landmark.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_point_2d.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_certificate.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_audio_frame.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_video_frame.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame_delegate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame_delegate.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame_attachment.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

bool V8ScriptValueSerializerForModules::WriteDOMObject(
    ScriptWrappable* wrappable,
    ExceptionState& exception_state) {
  // Give the core/ implementation a chance to try first.
  // If it didn't recognize the kind of wrapper, try the modules types.
  if (V8ScriptValueSerializer::WriteDOMObject(wrappable, exception_state))
    return true;
  if (exception_state.HadException())
    return false;

  const WrapperTypeInfo* wrapper_type_info = wrappable->GetWrapperTypeInfo();
  if (wrapper_type_info == V8CryptoKey::GetWrapperTypeInfo()) {
    return WriteCryptoKey(wrappable->ToImpl<CryptoKey>()->Key(),
                          exception_state);
  }
  if (wrapper_type_info == V8DOMFileSystem::GetWrapperTypeInfo()) {
    DOMFileSystem* fs = wrappable->ToImpl<DOMFileSystem>();
    if (!fs->Clonable()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "A FileSystem object could not be cloned.");
      return false;
    }
    WriteTag(kDOMFileSystemTag);
    // This locks in the values of the FileSystemType enumerators.
    WriteUint32(static_cast<uint32_t>(fs->GetType()));
    WriteUTF8String(fs->name());
    WriteUTF8String(fs->RootURL().GetString());
    return true;
  }
  if (wrapper_type_info == V8FileSystemFileHandle::GetWrapperTypeInfo() &&
      RuntimeEnabledFeatures::NativeFileSystemEnabled(
          ExecutionContext::From(GetScriptState()))) {
    return WriteNativeFileSystemHandle(
        kNativeFileSystemFileHandleTag,
        wrappable->ToImpl<NativeFileSystemHandle>());
  }
  if (wrapper_type_info == V8FileSystemDirectoryHandle::GetWrapperTypeInfo() &&
      RuntimeEnabledFeatures::NativeFileSystemEnabled(
          ExecutionContext::From(GetScriptState()))) {
    return WriteNativeFileSystemHandle(
        kNativeFileSystemDirectoryHandleTag,
        wrappable->ToImpl<NativeFileSystemHandle>());
  }
  if (wrapper_type_info == V8RTCCertificate::GetWrapperTypeInfo()) {
    RTCCertificate* certificate = wrappable->ToImpl<RTCCertificate>();
    rtc::RTCCertificatePEM pem = certificate->Certificate()->ToPEM();
    WriteTag(kRTCCertificateTag);
    WriteUTF8String(pem.private_key().c_str());
    WriteUTF8String(pem.certificate().c_str());
    return true;
  }
  if (wrapper_type_info == V8RTCEncodedAudioFrame::GetWrapperTypeInfo()) {
    if (IsForStorage()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                        "An RTCEncodedAudioFrame cannot be "
                                        "serialized for storage.");
      return false;
    }
    return WriteRTCEncodedAudioFrame(wrappable->ToImpl<RTCEncodedAudioFrame>());
  }
  if (wrapper_type_info == V8RTCEncodedVideoFrame::GetWrapperTypeInfo()) {
    if (IsForStorage()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                        "An RTCEncodedVideoFrame cannot be "
                                        "serialized for storage.");
      return false;
    }
    return WriteRTCEncodedVideoFrame(wrappable->ToImpl<RTCEncodedVideoFrame>());
  }
  if (wrapper_type_info == V8VideoFrame::GetWrapperTypeInfo() &&
      RuntimeEnabledFeatures::WebCodecsEnabled(
          ExecutionContext::From(GetScriptState()))) {
    if (IsForStorage()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                        "A VideoFrame cannot be serialized for "
                                        "storage.");
      return false;
    }
    auto* video_frame = wrappable->ToImpl<VideoFrame>();

    if (!video_frame->frame()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "Cannot serialize destroyed VideoFrame.");
      return false;
    }

    return WriteVideoFrame(video_frame);
  }
  return false;
}

namespace {

uint32_t AlgorithmIdForWireFormat(WebCryptoAlgorithmId id) {
  switch (id) {
    case kWebCryptoAlgorithmIdAesCbc:
      return kAesCbcTag;
    case kWebCryptoAlgorithmIdHmac:
      return kHmacTag;
    case kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
      return kRsaSsaPkcs1v1_5Tag;
    case kWebCryptoAlgorithmIdSha1:
      return kSha1Tag;
    case kWebCryptoAlgorithmIdSha256:
      return kSha256Tag;
    case kWebCryptoAlgorithmIdSha384:
      return kSha384Tag;
    case kWebCryptoAlgorithmIdSha512:
      return kSha512Tag;
    case kWebCryptoAlgorithmIdAesGcm:
      return kAesGcmTag;
    case kWebCryptoAlgorithmIdRsaOaep:
      return kRsaOaepTag;
    case kWebCryptoAlgorithmIdAesCtr:
      return kAesCtrTag;
    case kWebCryptoAlgorithmIdAesKw:
      return kAesKwTag;
    case kWebCryptoAlgorithmIdRsaPss:
      return kRsaPssTag;
    case kWebCryptoAlgorithmIdEcdsa:
      return kEcdsaTag;
    case kWebCryptoAlgorithmIdEcdh:
      return kEcdhTag;
    case kWebCryptoAlgorithmIdHkdf:
      return kHkdfTag;
    case kWebCryptoAlgorithmIdPbkdf2:
      return kPbkdf2Tag;
    // TODO(crbug.com/1032821): Handle them explicitly for Lint.
    case kWebCryptoAlgorithmIdEd25519:
    case kWebCryptoAlgorithmIdX25519:
      return 0;
  }
  NOTREACHED() << "Unknown algorithm ID " << id;
  return 0;
}

uint32_t AsymmetricKeyTypeForWireFormat(WebCryptoKeyType key_type) {
  switch (key_type) {
    case kWebCryptoKeyTypePublic:
      return kPublicKeyType;
    case kWebCryptoKeyTypePrivate:
      return kPrivateKeyType;
    case kWebCryptoKeyTypeSecret:
      break;
  }
  NOTREACHED() << "Unknown asymmetric key type " << key_type;
  return 0;
}

uint32_t NamedCurveForWireFormat(WebCryptoNamedCurve named_curve) {
  switch (named_curve) {
    case kWebCryptoNamedCurveP256:
      return kP256Tag;
    case kWebCryptoNamedCurveP384:
      return kP384Tag;
    case kWebCryptoNamedCurveP521:
      return kP521Tag;
  }
  NOTREACHED() << "Unknown named curve " << named_curve;
  return 0;
}

uint32_t KeyUsagesForWireFormat(WebCryptoKeyUsageMask usages,
                                bool extractable) {
  // Reminder to update this when adding new key usages.
  static_assert(kEndOfWebCryptoKeyUsage == (1 << 7) + 1,
                "update required when adding new key usages");
  uint32_t value = 0;
  if (extractable)
    value |= kExtractableUsage;
  if (usages & kWebCryptoKeyUsageEncrypt)
    value |= kEncryptUsage;
  if (usages & kWebCryptoKeyUsageDecrypt)
    value |= kDecryptUsage;
  if (usages & kWebCryptoKeyUsageSign)
    value |= kSignUsage;
  if (usages & kWebCryptoKeyUsageVerify)
    value |= kVerifyUsage;
  if (usages & kWebCryptoKeyUsageDeriveKey)
    value |= kDeriveKeyUsage;
  if (usages & kWebCryptoKeyUsageWrapKey)
    value |= kWrapKeyUsage;
  if (usages & kWebCryptoKeyUsageUnwrapKey)
    value |= kUnwrapKeyUsage;
  if (usages & kWebCryptoKeyUsageDeriveBits)
    value |= kDeriveBitsUsage;
  return value;
}

}  // namespace

bool V8ScriptValueSerializerForModules::WriteCryptoKey(
    const WebCryptoKey& key,
    ExceptionState& exception_state) {
  WriteTag(kCryptoKeyTag);

  // Write params.
  const WebCryptoKeyAlgorithm& algorithm = key.Algorithm();
  switch (algorithm.ParamsType()) {
    case kWebCryptoKeyAlgorithmParamsTypeAes: {
      const auto& params = *algorithm.AesParams();
      WriteOneByte(kAesKeyTag);
      WriteUint32(AlgorithmIdForWireFormat(algorithm.Id()));
      DCHECK_EQ(0, params.LengthBits() % 8);
      WriteUint32(params.LengthBits() / 8);
      break;
    }
    case kWebCryptoKeyAlgorithmParamsTypeHmac: {
      const auto& params = *algorithm.HmacParams();
      WriteOneByte(kHmacKeyTag);
      DCHECK_EQ(0u, params.LengthBits() % 8);
      WriteUint32(params.LengthBits() / 8);
      WriteUint32(AlgorithmIdForWireFormat(params.GetHash().Id()));
      break;
    }
    case kWebCryptoKeyAlgorithmParamsTypeRsaHashed: {
      const auto& params = *algorithm.RsaHashedParams();
      WriteOneByte(kRsaHashedKeyTag);
      WriteUint32(AlgorithmIdForWireFormat(algorithm.Id()));
      WriteUint32(AsymmetricKeyTypeForWireFormat(key.GetType()));
      WriteUint32(params.ModulusLengthBits());

      if (params.PublicExponent().size() >
          std::numeric_limits<uint32_t>::max()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kDataCloneError,
            "A CryptoKey object could not be cloned.");
        return false;
      }
      WriteUint32(static_cast<uint32_t>(params.PublicExponent().size()));
      WriteRawBytes(params.PublicExponent().Data(),
                    params.PublicExponent().size());
      WriteUint32(AlgorithmIdForWireFormat(params.GetHash().Id()));
      break;
    }
    case kWebCryptoKeyAlgorithmParamsTypeEc: {
      const auto& params = *algorithm.EcParams();
      WriteOneByte(kEcKeyTag);
      WriteUint32(AlgorithmIdForWireFormat(algorithm.Id()));
      WriteUint32(AsymmetricKeyTypeForWireFormat(key.GetType()));
      WriteUint32(NamedCurveForWireFormat(params.NamedCurve()));
      break;
    }
    case kWebCryptoKeyAlgorithmParamsTypeNone:
      DCHECK(WebCryptoAlgorithm::IsKdf(algorithm.Id()));
      WriteOneByte(kNoParamsKeyTag);
      WriteUint32(AlgorithmIdForWireFormat(algorithm.Id()));
      break;
  }

  // Write key usages.
  WriteUint32(KeyUsagesForWireFormat(key.Usages(), key.Extractable()));

  // Write key data.
  WebVector<uint8_t> key_data;
  if (!Platform::Current()->Crypto()->SerializeKeyForClone(key, key_data) ||
      key_data.size() > std::numeric_limits<uint32_t>::max()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataCloneError,
        "A CryptoKey object could not be cloned.");
    return false;
  }
  WriteUint32(static_cast<uint32_t>(key_data.size()));
  WriteRawBytes(key_data.Data(), key_data.size());

  return true;
}

bool V8ScriptValueSerializerForModules::WriteNativeFileSystemHandle(
    SerializationTag tag,
    NativeFileSystemHandle* native_file_system_handle) {
  mojo::PendingRemote<mojom::blink::NativeFileSystemTransferToken> token =
      native_file_system_handle->Transfer();

  SerializedScriptValue::NativeFileSystemTokensArray& tokens_array =
      GetSerializedScriptValue()->NativeFileSystemTokens();

  tokens_array.push_back(std::move(token));
  const uint32_t token_index = static_cast<uint32_t>(tokens_array.size() - 1);

  WriteTag(tag);
  WriteUTF8String(native_file_system_handle->name());
  WriteUint32(token_index);
  return true;
}

bool V8ScriptValueSerializerForModules::WriteRTCEncodedAudioFrame(
    RTCEncodedAudioFrame* audio_frame) {
  auto* attachment =
      GetSerializedScriptValue()
          ->GetOrCreateAttachment<RTCEncodedAudioFramesAttachment>();
  auto& frames = attachment->EncodedAudioFrames();
  frames.push_back(audio_frame->Delegate());
  const uint32_t index = static_cast<uint32_t>(frames.size() - 1);

  WriteTag(kRTCEncodedAudioFrameTag);
  WriteUint32(index);
  return true;
}

bool V8ScriptValueSerializerForModules::WriteRTCEncodedVideoFrame(
    RTCEncodedVideoFrame* video_frame) {
  auto* attachment =
      GetSerializedScriptValue()
          ->GetOrCreateAttachment<RTCEncodedVideoFramesAttachment>();
  auto& frames = attachment->EncodedVideoFrames();
  frames.push_back(video_frame->Delegate());
  const uint32_t index = static_cast<uint32_t>(frames.size() - 1);

  WriteTag(kRTCEncodedVideoFrameTag);
  WriteUint32(index);
  return true;
}

bool V8ScriptValueSerializerForModules::WriteVideoFrame(
    VideoFrame* video_frame) {
  auto* attachment =
      GetSerializedScriptValue()->GetOrCreateAttachment<VideoFrameAttachment>();
  auto& frames = attachment->Handles();
  frames.push_back(video_frame->handle());
  const uint32_t index = static_cast<uint32_t>(frames.size() - 1);

  WriteTag(kVideoFrameTag);
  WriteUint32(index);

  return true;
}

}  // namespace blink
