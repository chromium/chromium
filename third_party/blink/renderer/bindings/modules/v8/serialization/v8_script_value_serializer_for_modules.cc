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
#include "third_party/blink/renderer/bindings/modules/v8/v8_detected_barcode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_detected_face.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_detected_text.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_dom_file_system.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_directory_handle.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_file_handle.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_certificate.h"
#include "third_party/blink/renderer/modules/imagecapture/point_2d.h"
#include "third_party/blink/renderer/modules/shapedetection/landmark.h"
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
      RuntimeEnabledFeatures::CloneableNativeFileSystemHandlesEnabled()) {
    if (IsForStorage()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "A NativeFileSystemFileHandle can not be serialized for storage.");
      return false;
    }
    return WriteNativeFileSystemHandle(
        kNativeFileSystemFileHandleTag,
        wrappable->ToImpl<NativeFileSystemHandle>());
  }
  if (wrapper_type_info == V8FileSystemDirectoryHandle::GetWrapperTypeInfo() &&
      RuntimeEnabledFeatures::CloneableNativeFileSystemHandlesEnabled()) {
    if (IsForStorage()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                        "A NativeFileSystemDirectoryHandle can "
                                        "not be serialized for storage.");
      return false;
    }
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
  if (wrapper_type_info == V8DetectedBarcode::GetWrapperTypeInfo()) {
    DetectedBarcode* detected_barcode = wrappable->ToImpl<DetectedBarcode>();
    WriteTag(kDetectedBarcodeTag);
    WriteUTF8String(detected_barcode->rawValue());
    DOMRectReadOnly* bounding_box = detected_barcode->boundingBox();
    WriteDouble(bounding_box->x());
    WriteDouble(bounding_box->y());
    WriteDouble(bounding_box->width());
    WriteDouble(bounding_box->height());
    const HeapVector<Member<Point2D>>& corner_points =
        detected_barcode->cornerPoints();
    WriteUint32(static_cast<uint32_t>(corner_points.size()));
    for (const auto& corner_point : corner_points) {
      WriteDouble(corner_point->x());
      WriteDouble(corner_point->y());
    }
    return true;
  }
  if (wrapper_type_info == V8DetectedFace::GetWrapperTypeInfo()) {
    DetectedFace* detected_face = wrappable->ToImpl<DetectedFace>();
    WriteTag(kDetectedFaceTag);
    DOMRectReadOnly* bounding_box = detected_face->boundingBox();
    WriteDouble(bounding_box->x());
    WriteDouble(bounding_box->y());
    WriteDouble(bounding_box->width());
    WriteDouble(bounding_box->height());
    const HeapVector<Member<Landmark>>& landmarks = detected_face->landmarks();
    WriteUint32(static_cast<uint32_t>(landmarks.size()));
    for (const auto& landmark : landmarks) {
      WriteUTF8String(landmark->type());
      const HeapVector<Member<Point2D>>& locations = landmark->locations();
      WriteUint32(static_cast<uint32_t>(locations.size()));
      for (const auto& location : locations) {
        WriteDouble(location->x());
        WriteDouble(location->y());
      }
    }
    return true;
  }
  if (wrapper_type_info == V8DetectedText::GetWrapperTypeInfo()) {
    DetectedText* detected_text = wrappable->ToImpl<DetectedText>();
    WriteTag(kDetectedTextTag);
    WriteUTF8String(detected_text->rawValue());
    DOMRectReadOnly* bounding_box = detected_text->boundingBox();
    WriteDouble(bounding_box->x());
    WriteDouble(bounding_box->y());
    WriteDouble(bounding_box->width());
    WriteDouble(bounding_box->height());
    const HeapVector<Member<Point2D>>& corner_points =
        detected_text->cornerPoints();
    WriteUint32(static_cast<uint32_t>(corner_points.size()));
    for (const auto& corner_point : corner_points) {
      WriteDouble(corner_point->x());
      WriteDouble(corner_point->y());
    }
    return true;
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

}  // namespace blink
