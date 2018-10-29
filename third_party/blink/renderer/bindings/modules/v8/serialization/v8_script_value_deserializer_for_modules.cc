// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/modules/v8/serialization/v8_script_value_deserializer_for_modules.h"

#include "third_party/blink/public/mojom/filesystem/file_system.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_crypto.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"
#include "third_party/blink/public/platform/web_rtc_certificate_generator.h"
#include "third_party/blink/renderer/bindings/modules/v8/serialization/web_crypto_sub_tags.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/crypto/crypto_key.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_certificate.h"
#include "third_party/blink/renderer/modules/shapedetection/detected_barcode.h"
#include "third_party/blink/renderer/modules/shapedetection/detected_face.h"
#include "third_party/blink/renderer/modules/shapedetection/detected_text.h"

namespace blink {

ScriptWrappable* V8ScriptValueDeserializerForModules::ReadDOMObject(
    SerializationTag tag) {
  // Give the core/ implementation a chance to try first.
  // If it didn't recognize the kind of wrapper, try the modules types.
  if (ScriptWrappable* wrappable =
          V8ScriptValueDeserializer::ReadDOMObject(tag))
    return wrappable;

  switch (tag) {
    case kCryptoKeyTag:
      return ReadCryptoKey();
    case kDOMFileSystemTag: {
      uint32_t raw_type;
      String name;
      String root_url;
      if (!ReadUint32(&raw_type) ||
          raw_type >
              static_cast<int32_t>(mojom::blink::FileSystemType::kMaxValue) ||
          !ReadUTF8String(&name) || !ReadUTF8String(&root_url))
        return nullptr;
      return DOMFileSystem::Create(
          ExecutionContext::From(GetScriptState()), name,
          static_cast<mojom::blink::FileSystemType>(raw_type), KURL(root_url));
    }
    case kRTCCertificateTag: {
      String pem_private_key;
      String pem_certificate;
      if (!ReadUTF8String(&pem_private_key) ||
          !ReadUTF8String(&pem_certificate))
        return nullptr;
      std::unique_ptr<WebRTCCertificateGenerator> certificate_generator(
          Platform::Current()->CreateRTCCertificateGenerator());
      if (!certificate_generator)
        return nullptr;
      rtc::scoped_refptr<rtc::RTCCertificate> certificate =
          certificate_generator->FromPEM(pem_private_key, pem_certificate);
      if (!certificate)
        return nullptr;
      return new RTCCertificate(std::move(certificate));
    }
    case kDetectedBarcodeTag: {
      String raw_value;
      if (!ReadUTF8String(&raw_value))
        return nullptr;
      DOMRectReadOnly* bounding_box = ReadDOMRectReadOnly();
      if (!bounding_box)
        return nullptr;
      uint32_t corner_points_length;
      if (!ReadUint32(&corner_points_length))
        return nullptr;
      HeapVector<Point2D> corner_points;
      for (uint32_t i = 0; i < corner_points_length; i++) {
        Point2D point;
        if (!ReadPoint2D(&point))
          return nullptr;
        corner_points.push_back(point);
      }
      return DetectedBarcode::Create(raw_value, bounding_box, corner_points);
    }
    case kDetectedFaceTag: {
      DOMRectReadOnly* bounding_box = ReadDOMRectReadOnly();
      if (!bounding_box)
        return nullptr;
      uint32_t landmarks_length;
      if (!ReadUint32(&landmarks_length))
        return nullptr;
      HeapVector<Landmark> landmarks;
      for (uint32_t i = 0; i < landmarks_length; i++) {
        Landmark landmark;
        if (!ReadLandmark(&landmark))
          return nullptr;
        landmarks.push_back(landmark);
      }
      return DetectedFace::Create(bounding_box, landmarks);
    }
    case kDetectedTextTag: {
      String raw_value;
      if (!ReadUTF8String(&raw_value))
        return nullptr;
      DOMRectReadOnly* bounding_box = ReadDOMRectReadOnly();
      if (!bounding_box)
        return nullptr;
      uint32_t corner_points_length;
      if (!ReadUint32(&corner_points_length))
        return nullptr;
      HeapVector<Point2D> corner_points;
      for (uint32_t i = 0; i < corner_points_length; i++) {
        Point2D point;
        if (!ReadPoint2D(&point))
          return nullptr;
        corner_points.push_back(point);
      }
      return DetectedText::Create(raw_value, bounding_box, corner_points);
    }
    default:
      break;
  }
  return nullptr;
}

namespace {

bool AlgorithmIdFromWireFormat(uint32_t raw_id, WebCryptoAlgorithmId* id) {
  switch (static_cast<CryptoKeyAlgorithmTag>(raw_id)) {
    case kAesCbcTag:
      *id = kWebCryptoAlgorithmIdAesCbc;
      return true;
    case kHmacTag:
      *id = kWebCryptoAlgorithmIdHmac;
      return true;
    case kRsaSsaPkcs1v1_5Tag:
      *id = kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5;
      return true;
    case kSha1Tag:
      *id = kWebCryptoAlgorithmIdSha1;
      return true;
    case kSha256Tag:
      *id = kWebCryptoAlgorithmIdSha256;
      return true;
    case kSha384Tag:
      *id = kWebCryptoAlgorithmIdSha384;
      return true;
    case kSha512Tag:
      *id = kWebCryptoAlgorithmIdSha512;
      return true;
    case kAesGcmTag:
      *id = kWebCryptoAlgorithmIdAesGcm;
      return true;
    case kRsaOaepTag:
      *id = kWebCryptoAlgorithmIdRsaOaep;
      return true;
    case kAesCtrTag:
      *id = kWebCryptoAlgorithmIdAesCtr;
      return true;
    case kAesKwTag:
      *id = kWebCryptoAlgorithmIdAesKw;
      return true;
    case kRsaPssTag:
      *id = kWebCryptoAlgorithmIdRsaPss;
      return true;
    case kEcdsaTag:
      *id = kWebCryptoAlgorithmIdEcdsa;
      return true;
    case kEcdhTag:
      *id = kWebCryptoAlgorithmIdEcdh;
      return true;
    case kHkdfTag:
      *id = kWebCryptoAlgorithmIdHkdf;
      return true;
    case kPbkdf2Tag:
      *id = kWebCryptoAlgorithmIdPbkdf2;
      return true;
  }
  return false;
}

bool AsymmetricKeyTypeFromWireFormat(uint32_t raw_key_type,
                                     WebCryptoKeyType* key_type) {
  switch (static_cast<AsymmetricCryptoKeyType>(raw_key_type)) {
    case kPublicKeyType:
      *key_type = kWebCryptoKeyTypePublic;
      return true;
    case kPrivateKeyType:
      *key_type = kWebCryptoKeyTypePrivate;
      return true;
  }
  return false;
}

bool NamedCurveFromWireFormat(uint32_t raw_named_curve,
                              WebCryptoNamedCurve* named_curve) {
  switch (static_cast<NamedCurveTag>(raw_named_curve)) {
    case kP256Tag:
      *named_curve = kWebCryptoNamedCurveP256;
      return true;
    case kP384Tag:
      *named_curve = kWebCryptoNamedCurveP384;
      return true;
    case kP521Tag:
      *named_curve = kWebCryptoNamedCurveP521;
      return true;
  }
  return false;
}

bool KeyUsagesFromWireFormat(uint32_t raw_usages,
                             WebCryptoKeyUsageMask* usages,
                             bool* extractable) {
  // Reminder to update this when adding new key usages.
  static_assert(kEndOfWebCryptoKeyUsage == (1 << 7) + 1,
                "update required when adding new key usages");
  const uint32_t kAllPossibleUsages =
      kExtractableUsage | kEncryptUsage | kDecryptUsage | kSignUsage |
      kVerifyUsage | kDeriveKeyUsage | kWrapKeyUsage | kUnwrapKeyUsage |
      kDeriveBitsUsage;
  if (raw_usages & ~kAllPossibleUsages)
    return false;

  *usages = 0;
  *extractable = raw_usages & kExtractableUsage;
  if (raw_usages & kEncryptUsage)
    *usages |= kWebCryptoKeyUsageEncrypt;
  if (raw_usages & kDecryptUsage)
    *usages |= kWebCryptoKeyUsageDecrypt;
  if (raw_usages & kSignUsage)
    *usages |= kWebCryptoKeyUsageSign;
  if (raw_usages & kVerifyUsage)
    *usages |= kWebCryptoKeyUsageVerify;
  if (raw_usages & kDeriveKeyUsage)
    *usages |= kWebCryptoKeyUsageDeriveKey;
  if (raw_usages & kWrapKeyUsage)
    *usages |= kWebCryptoKeyUsageWrapKey;
  if (raw_usages & kUnwrapKeyUsage)
    *usages |= kWebCryptoKeyUsageUnwrapKey;
  if (raw_usages & kDeriveBitsUsage)
    *usages |= kWebCryptoKeyUsageDeriveBits;
  return true;
}

}  // namespace

CryptoKey* V8ScriptValueDeserializerForModules::ReadCryptoKey() {
  // Read params.
  uint8_t raw_key_type;
  if (!ReadOneByte(&raw_key_type))
    return nullptr;
  WebCryptoKeyAlgorithm algorithm;
  WebCryptoKeyType key_type = kWebCryptoKeyTypeSecret;
  switch (raw_key_type) {
    case kAesKeyTag: {
      uint32_t raw_id;
      WebCryptoAlgorithmId id;
      uint32_t length_bytes;
      if (!ReadUint32(&raw_id) || !AlgorithmIdFromWireFormat(raw_id, &id) ||
          !ReadUint32(&length_bytes) ||
          length_bytes > std::numeric_limits<unsigned short>::max() / 8u)
        return nullptr;
      algorithm = WebCryptoKeyAlgorithm::CreateAes(id, length_bytes * 8);
      key_type = kWebCryptoKeyTypeSecret;
      break;
    }
    case kHmacKeyTag: {
      uint32_t length_bytes;
      uint32_t raw_hash;
      WebCryptoAlgorithmId hash;
      if (!ReadUint32(&length_bytes) ||
          length_bytes > std::numeric_limits<unsigned>::max() / 8 ||
          !ReadUint32(&raw_hash) || !AlgorithmIdFromWireFormat(raw_hash, &hash))
        return nullptr;
      algorithm = WebCryptoKeyAlgorithm::CreateHmac(hash, length_bytes * 8);
      key_type = kWebCryptoKeyTypeSecret;
      break;
    }
    case kRsaHashedKeyTag: {
      uint32_t raw_id;
      WebCryptoAlgorithmId id;
      uint32_t raw_key_type;
      uint32_t modulus_length_bits;
      uint32_t public_exponent_size;
      const void* public_exponent_bytes;
      uint32_t raw_hash;
      WebCryptoAlgorithmId hash;
      if (!ReadUint32(&raw_id) || !AlgorithmIdFromWireFormat(raw_id, &id) ||
          !ReadUint32(&raw_key_type) ||
          !AsymmetricKeyTypeFromWireFormat(raw_key_type, &key_type) ||
          !ReadUint32(&modulus_length_bits) ||
          !ReadUint32(&public_exponent_size) ||
          !ReadRawBytes(public_exponent_size, &public_exponent_bytes) ||
          !ReadUint32(&raw_hash) || !AlgorithmIdFromWireFormat(raw_hash, &hash))
        return nullptr;
      algorithm = WebCryptoKeyAlgorithm::CreateRsaHashed(
          id, modulus_length_bits,
          reinterpret_cast<const unsigned char*>(public_exponent_bytes),
          public_exponent_size, hash);
      break;
    }
    case kEcKeyTag: {
      uint32_t raw_id;
      WebCryptoAlgorithmId id;
      uint32_t raw_key_type;
      uint32_t raw_named_curve;
      WebCryptoNamedCurve named_curve;
      if (!ReadUint32(&raw_id) || !AlgorithmIdFromWireFormat(raw_id, &id) ||
          !ReadUint32(&raw_key_type) ||
          !AsymmetricKeyTypeFromWireFormat(raw_key_type, &key_type) ||
          !ReadUint32(&raw_named_curve) ||
          !NamedCurveFromWireFormat(raw_named_curve, &named_curve))
        return nullptr;
      algorithm = WebCryptoKeyAlgorithm::CreateEc(id, named_curve);
      break;
    }
    case kNoParamsKeyTag: {
      uint32_t raw_id;
      WebCryptoAlgorithmId id;
      if (!ReadUint32(&raw_id) || !AlgorithmIdFromWireFormat(raw_id, &id))
        return nullptr;
      algorithm = WebCryptoKeyAlgorithm::CreateWithoutParams(id);
      break;
    }
  }
  if (algorithm.IsNull())
    return nullptr;

  // Read key usages.
  uint32_t raw_usages;
  WebCryptoKeyUsageMask usages;
  bool extractable;
  if (!ReadUint32(&raw_usages) ||
      !KeyUsagesFromWireFormat(raw_usages, &usages, &extractable))
    return nullptr;

  // Read key data.
  uint32_t key_data_length;
  const void* key_data;
  if (!ReadUint32(&key_data_length) ||
      !ReadRawBytes(key_data_length, &key_data))
    return nullptr;

  WebCryptoKey key = WebCryptoKey::CreateNull();
  if (!Platform::Current()->Crypto()->DeserializeKeyForClone(
          algorithm, key_type, extractable, usages,
          reinterpret_cast<const unsigned char*>(key_data), key_data_length,
          key))
    return nullptr;

  return CryptoKey::Create(key);
}

bool V8ScriptValueDeserializerForModules::ReadLandmark(Landmark* landmark) {
  String type;
  if (!ReadUTF8String(&type))
    return false;
  uint32_t locations_length;
  if (!ReadUint32(&locations_length))
    return false;
  HeapVector<Point2D> locations;
  for (uint32_t i = 0; i < locations_length; i++) {
    Point2D location;
    if (!ReadPoint2D(&location))
      return false;
    locations.push_back(location);
  }
  landmark->setType(type);
  landmark->setLocations(locations);
  return true;
}

bool V8ScriptValueDeserializerForModules::ReadPoint2D(Point2D* point) {
  double x = 0, y = 0;
  if (!ReadDouble(&x) || !ReadDouble(&y))
    return false;
  point->setX(x);
  point->setY(y);
  return true;
}

}  // namespace blink
