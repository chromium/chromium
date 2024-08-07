// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/modules/v8/serialization/v8_script_value_deserializer_for_modules.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_transfer_token.mojom-blink.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_crypto.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialization_tag.h"
#include "third_party/blink/renderer/bindings/modules/v8/serialization/serialized_track_params.h"
#include "third_party/blink/renderer/bindings/modules/v8/serialization/web_crypto_sub_tags.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_crop_target.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_crypto_key.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_dom_file_system.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_chunk.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_directory_handle.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_file_handle.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_source_handle.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_track.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_restriction_target.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_certificate.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_data_channel.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_audio_frame.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_video_frame.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/crypto/crypto_key.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_directory_handle.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_file_handle.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system.h"
#include "third_party/blink/renderer/modules/mediasource/media_source_attachment_supplement.h"
#include "third_party/blink/renderer/modules/mediasource/media_source_handle_attachment.h"
#include "third_party/blink/renderer/modules/mediasource/media_source_handle_impl.h"
#include "third_party/blink/renderer/modules/mediastream/crop_target.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/restriction_target.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_certificate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_certificate_generator.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_data_channel.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_data_channel_attachment.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame_delegate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame_delegate.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_data.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_data_attachment.h"
#include "third_party/blink/renderer/modules/webcodecs/decoder_buffer_attachment.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_audio_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame_attachment.h"

namespace blink {

ScriptWrappable* V8ScriptValueDeserializerForModules::ReadDOMObject(
    SerializationTag tag,
    ExceptionState& exception_state) {
  // Give the core/ implementation a chance to try first.
  // If it didn't recognize the kind of wrapper, try the modules types.
  if (ScriptWrappable* wrappable =
          V8ScriptValueDeserializer::ReadDOMObject(tag, exception_state))
    return wrappable;

  if (!ExecutionContextExposesInterface(
          ExecutionContext::From(GetScriptState()), tag)) {
    return nullptr;
  }
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
      return MakeGarbageCollected<DOMFileSystem>(
          ExecutionContext::From(GetScriptState()), name,
          static_cast<mojom::blink::FileSystemType>(raw_type), KURL(root_url));
    }
    case kFileSystemFileHandleTag:
    case kFileSystemDirectoryHandleTag:
      return ReadFileSystemHandle(tag);
    case kRTCCertificateTag: {
      String pem_private_key;
      String pem_certificate;
      if (!ReadUTF8String(&pem_private_key) ||
          !ReadUTF8String(&pem_certificate))
        return nullptr;
      std::unique_ptr<RTCCertificateGenerator> certificate_generator =
          std::make_unique<RTCCertificateGenerator>();
      if (!certificate_generator)
        return nullptr;
      rtc::scoped_refptr<rtc::RTCCertificate> certificate =
          certificate_generator->FromPEM(pem_private_key, pem_certificate);
      if (!certificate)
        return nullptr;
      return MakeGarbageCollected<RTCCertificate>(std::move(certificate));
    }
    case kRTCDataChannel:
      return ReadRTCDataChannel();
    case kRTCEncodedAudioFrameTag:
      return ReadRTCEncodedAudioFrame();
    case kRTCEncodedVideoFrameTag:
      return ReadRTCEncodedVideoFrame();
    case kAudioDataTag:
      return ReadAudioData();
    case kVideoFrameTag:
      return ReadVideoFrame();
    case kEncodedAudioChunkTag:
      return ReadEncodedAudioChunk();
    case kEncodedVideoChunkTag:
      return ReadEncodedVideoChunk();
    case kMediaStreamTrack:
      return ReadMediaStreamTrack();
    case kCropTargetTag:
      return ReadCropTarget();
    case kRestrictionTargetTag:
      return ReadRestrictionTarget();
    case kMediaSourceHandleTag:
      return ReadMediaSourceHandle();
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
    case kEd25519Tag:
      *id = kWebCryptoAlgorithmIdEd25519;
      return true;
    case kX25519Tag:
      *id = kWebCryptoAlgorithmIdX25519;
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
  uint8_t raw_key_byte;
  if (!ReadOneByte(&raw_key_byte))
    return nullptr;
  WebCryptoKeyAlgorithm algorithm;
  WebCryptoKeyType key_type = kWebCryptoKeyTypeSecret;
  switch (raw_key_byte) {
    case kAesKeyTag: {
      uint32_t raw_id;
      WebCryptoAlgorithmId id;
      uint32_t length_bytes;
      if (!ReadUint32(&raw_id) || !AlgorithmIdFromWireFormat(raw_id, &id) ||
          !ReadUint32(&length_bytes) ||
          length_bytes > std::numeric_limits<uint16_t>::max() / 8u)
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
    case kEd25519KeyTag:
    case kX25519KeyTag: {
      if (!RuntimeEnabledFeatures::WebCryptoCurve25519Enabled())
        break;
      uint32_t raw_id;
      WebCryptoAlgorithmId id;
      uint32_t raw_key_type;
      if (!ReadUint32(&raw_id) || !AlgorithmIdFromWireFormat(raw_id, &id) ||
          !ReadUint32(&raw_key_type) ||
          !AsymmetricKeyTypeFromWireFormat(raw_key_type, &key_type))
        return nullptr;
      algorithm = raw_key_byte == kEd25519KeyTag
                      ? WebCryptoKeyAlgorithm::CreateEd25519(id)
                      : WebCryptoKeyAlgorithm::CreateX25519(id);
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

  return MakeGarbageCollected<CryptoKey>(key);
}

FileSystemHandle* V8ScriptValueDeserializerForModules::ReadFileSystemHandle(
    SerializationTag tag) {
  if (!RuntimeEnabledFeatures::FileSystemAccessEnabled(
          ExecutionContext::From(GetScriptState()))) {
    return nullptr;
  }

  String name;
  uint32_t token_index;
  if (!ReadUTF8String(&name) || !ReadUint32(&token_index)) {
    return nullptr;
  }

  // Find the FileSystemHandle's token.
  SerializedScriptValue::FileSystemAccessTokensArray& tokens_array =
      GetSerializedScriptValue()->FileSystemAccessTokens();
  if (token_index >= tokens_array.size()) {
    return nullptr;
  }

  // IndexedDB code assumes that deserializing a SSV is non-destructive. So
  // rather than consuming the token here instead we clone it.
  mojo::Remote<mojom::blink::FileSystemAccessTransferToken> token(
      std::move(tokens_array[token_index]));
  if (!token) {
    return nullptr;
  }

  mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken> token_clone;
  token->Clone(token_clone.InitWithNewPipeAndPassReceiver());
  tokens_array[token_index] = std::move(token_clone);

  // Use the FileSystemAccessManager to redeem the token to clone the
  // FileSystemHandle.
  ExecutionContext* execution_context =
      ExecutionContext::From(GetScriptState());
  mojo::Remote<mojom::blink::FileSystemAccessManager>
      file_system_access_manager;
  execution_context->GetBrowserInterfaceBroker().GetInterface(
      file_system_access_manager.BindNewPipeAndPassReceiver());

  // Clone the FileSystemHandle object.
  switch (tag) {
    case kFileSystemFileHandleTag: {
      mojo::PendingRemote<mojom::blink::FileSystemAccessFileHandle> file_handle;

      file_system_access_manager->GetFileHandleFromToken(
          token.Unbind(), file_handle.InitWithNewPipeAndPassReceiver());

      return MakeGarbageCollected<FileSystemFileHandle>(execution_context, name,
                                                        std::move(file_handle));
    }
    case kFileSystemDirectoryHandleTag: {
      mojo::PendingRemote<mojom::blink::FileSystemAccessDirectoryHandle>
          directory_handle;

      file_system_access_manager->GetDirectoryHandleFromToken(
          token.Unbind(), directory_handle.InitWithNewPipeAndPassReceiver());

      return MakeGarbageCollected<FileSystemDirectoryHandle>(
          execution_context, name, std::move(directory_handle));
    }
    default: {
      NOTREACHED_IN_MIGRATION();
      return nullptr;
    }
  }
}

RTCDataChannel* V8ScriptValueDeserializerForModules::ReadRTCDataChannel() {
  if (!RuntimeEnabledFeatures::TransferableRTCDataChannelEnabled(
          ExecutionContext::From(GetScriptState()))) {
    return nullptr;
  }

  uint32_t index;
  if (!ReadUint32(&index)) {
    return nullptr;
  }

  const auto* attachment =
      GetSerializedScriptValue()
          ->GetAttachmentIfExists<RTCDataChannelAttachment>();
  if (!attachment) {
    return nullptr;
  }

  using NativeDataChannelVector =
      Vector<rtc::scoped_refptr<webrtc::DataChannelInterface>>;

  const NativeDataChannelVector& channels = attachment->DataChannels();
  if (index >= attachment->size() || !channels[index]) {
    return nullptr;
  }

  RTCDataChannel::EnsureThreadWrappersForWorkerThread();

  return MakeGarbageCollected<RTCDataChannel>(
      ExecutionContext::From(GetScriptState()), std::move(channels[index]));
}

RTCEncodedAudioFrame*
V8ScriptValueDeserializerForModules::ReadRTCEncodedAudioFrame() {
  uint32_t index;
  if (!ReadUint32(&index))
    return nullptr;

  const auto* attachment =
      GetSerializedScriptValue()
          ->GetAttachmentIfExists<RTCEncodedAudioFramesAttachment>();
  if (!attachment)
    return nullptr;

  const auto& frames = attachment->EncodedAudioFrames();
  if (index >= frames.size())
    return nullptr;

  return MakeGarbageCollected<RTCEncodedAudioFrame>(frames[index]);
}

RTCEncodedVideoFrame*
V8ScriptValueDeserializerForModules::ReadRTCEncodedVideoFrame() {
  uint32_t index;
  if (!ReadUint32(&index))
    return nullptr;

  const auto* attachment =
      GetSerializedScriptValue()
          ->GetAttachmentIfExists<RTCEncodedVideoFramesAttachment>();
  if (!attachment)
    return nullptr;

  const auto& frames = attachment->EncodedVideoFrames();
  if (index >= frames.size())
    return nullptr;

  return MakeGarbageCollected<RTCEncodedVideoFrame>(frames[index]);
}

AudioData* V8ScriptValueDeserializerForModules::ReadAudioData() {
  uint32_t index;
  if (!ReadUint32(&index))
    return nullptr;

  const auto* attachment =
      GetSerializedScriptValue()->GetAttachmentIfExists<AudioDataAttachment>();
  if (!attachment)
    return nullptr;

  const auto& audio_buffers = attachment->AudioBuffers();
  if (index >= attachment->size())
    return nullptr;

  return MakeGarbageCollected<AudioData>(audio_buffers[index]);
}

VideoFrame* V8ScriptValueDeserializerForModules::ReadVideoFrame() {
  uint32_t index;
  if (!ReadUint32(&index))
    return nullptr;

  const auto* attachment =
      GetSerializedScriptValue()->GetAttachmentIfExists<VideoFrameAttachment>();
  if (!attachment)
    return nullptr;

  const auto& handles = attachment->Handles();
  if (index >= attachment->size())
    return nullptr;

  return MakeGarbageCollected<VideoFrame>(handles[index]);
}

EncodedAudioChunk*
V8ScriptValueDeserializerForModules::ReadEncodedAudioChunk() {
  uint32_t index;
  if (!ReadUint32(&index))
    return nullptr;

  const auto* attachment =
      GetSerializedScriptValue()
          ->GetAttachmentIfExists<DecoderBufferAttachment>();
  if (!attachment)
    return nullptr;

  const auto& buffers = attachment->Buffers();
  if (index >= attachment->size())
    return nullptr;

  return MakeGarbageCollected<EncodedAudioChunk>(buffers[index]);
}

EncodedVideoChunk*
V8ScriptValueDeserializerForModules::ReadEncodedVideoChunk() {
  uint32_t index;
  if (!ReadUint32(&index))
    return nullptr;

  const auto* attachment =
      GetSerializedScriptValue()
          ->GetAttachmentIfExists<DecoderBufferAttachment>();
  if (!attachment)
    return nullptr;

  const auto& buffers = attachment->Buffers();
  if (index >= attachment->size())
    return nullptr;

  return MakeGarbageCollected<EncodedVideoChunk>(buffers[index]);
}

MediaStreamTrack* V8ScriptValueDeserializerForModules::ReadMediaStreamTrack() {
  if (!RuntimeEnabledFeatures::MediaStreamTrackTransferEnabled(
          ExecutionContext::From(GetScriptState()))) {
    return nullptr;
  }

  base::UnguessableToken session_id, transfer_id;
  String kind, id, label;
  uint8_t enabled, muted;
  SerializedTrackImplSubtype track_impl_subtype;
  SerializedContentHintType contentHint;
  SerializedReadyState readyState;

  if (!ReadUint32Enum(&track_impl_subtype) ||
      !ReadUnguessableToken(&session_id) ||
      !ReadUnguessableToken(&transfer_id) || !ReadUTF8String(&kind) ||
      (kind != "audio" && kind != "video") || !ReadUTF8String(&id) ||
      !ReadUTF8String(&label) || !ReadOneByte(&enabled) || enabled > 1 ||
      !ReadOneByte(&muted) || muted > 1 || !ReadUint32Enum(&contentHint) ||
      !ReadUint32Enum(&readyState)) {
    return nullptr;
  }

  std::optional<uint32_t> sub_capture_target_version;
  // Using `switch` to ensure new enum values are handled.
  switch (track_impl_subtype) {
    case SerializedTrackImplSubtype::kTrackImplSubtypeBase:
      // No additional data to be deserialized.
      break;
    case SerializedTrackImplSubtype::kTrackImplSubtypeCanvasCapture:
    case SerializedTrackImplSubtype::kTrackImplSubtypeGenerator:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
    case SerializedTrackImplSubtype::kTrackImplSubtypeBrowserCapture:
      uint32_t read_sub_capture_target_version;
      if (!ReadUint32(&read_sub_capture_target_version)) {
        return nullptr;
      }
      sub_capture_target_version = read_sub_capture_target_version;
      break;
  }

  return MediaStreamTrack::FromTransferredState(
      GetScriptState(),
      MediaStreamTrack::TransferredValues{
          .track_impl_subtype = DeserializeTrackImplSubtype(track_impl_subtype),
          .session_id = session_id,
          .transfer_id = transfer_id,
          .kind = kind,
          .id = id,
          .label = label,
          .enabled = static_cast<bool>(enabled),
          .muted = static_cast<bool>(muted),
          .content_hint = DeserializeContentHint(contentHint),
          .ready_state = DeserializeReadyState(readyState),
          .sub_capture_target_version = sub_capture_target_version});
}

CropTarget* V8ScriptValueDeserializerForModules::ReadCropTarget() {
  if (!RuntimeEnabledFeatures::RegionCaptureEnabled(
          ExecutionContext::From(GetScriptState()))) {
    return nullptr;
  }

  String crop_id;
  if (!ReadUTF8String(&crop_id) || crop_id.empty()) {
    return nullptr;
  }

  return MakeGarbageCollected<CropTarget>(crop_id);
}

RestrictionTarget*
V8ScriptValueDeserializerForModules::ReadRestrictionTarget() {
  if (!RuntimeEnabledFeatures::ElementCaptureEnabled(
          ExecutionContext::From(GetScriptState()))) {
    return nullptr;
  }

  String restriction_id;
  if (!ReadUTF8String(&restriction_id) || restriction_id.empty()) {
    return nullptr;
  }

  return MakeGarbageCollected<RestrictionTarget>(restriction_id);
}

MediaSourceHandleImpl*
V8ScriptValueDeserializerForModules::ReadMediaSourceHandle() {
  uint32_t index;
  if (!ReadUint32(&index))
    return nullptr;

  const auto* attachment =
      GetSerializedScriptValue()
          ->GetAttachmentIfExists<MediaSourceHandleAttachment>();
  if (!attachment)
    return nullptr;

  const auto& attachments = attachment->Attachments();
  if (index >= attachment->size())
    return nullptr;

  auto& handle_internals = attachments[index];
  return MakeGarbageCollected<MediaSourceHandleImpl>(
      std::move(handle_internals.attachment_provider),
      std::move(handle_internals.internal_blob_url));
}

// static
bool V8ScriptValueDeserializerForModules::ExecutionContextExposesInterface(
    ExecutionContext* execution_context,
    SerializationTag interface_tag) {
  // If you're updating this, consider whether you should also update
  // V8ScriptValueSerializerForModules to call
  // TrailerWriter::RequireExposedInterface (generally via
  // WriteAndRequireInterfaceTag). Any interface which might potentially not be
  // exposed on all realms, even if not currently (i.e., most or all) should
  // probably be listed here.
  if (V8ScriptValueDeserializer::ExecutionContextExposesInterface(
          execution_context, interface_tag)) {
    return true;
  }
  switch (interface_tag) {
    case kCryptoKeyTag:
      return V8CryptoKey::IsExposed(execution_context);
    case kDOMFileSystemTag:
      // TODO(crbug.com/1366065): In theory this should be the result of
      // V8DOMFileSystem::IsExposed, but that's actually _nowhere_ right now.
      // This is an attempt to preserve things that might be working while
      // someone with actual file system API expertise looks into it.
      return execution_context->IsWindow() ||
             execution_context->IsWorkerGlobalScope();
    case kFileSystemFileHandleTag:
      return V8FileSystemFileHandle::IsExposed(execution_context);
    case kFileSystemDirectoryHandleTag:
      return V8FileSystemDirectoryHandle::IsExposed(execution_context);
    case kRTCCertificateTag:
      return V8RTCCertificate::IsExposed(execution_context);
    case kRTCEncodedAudioFrameTag:
      return V8RTCEncodedAudioFrame::IsExposed(execution_context);
    case kRTCEncodedVideoFrameTag:
      return V8RTCEncodedVideoFrame::IsExposed(execution_context);
    case kRTCDataChannel:
      return V8RTCDataChannel::IsExposed(execution_context);
    case kAudioDataTag:
      return V8AudioData::IsExposed(execution_context);
    case kVideoFrameTag:
      return V8VideoFrame::IsExposed(execution_context);
    case kEncodedAudioChunkTag:
      return V8EncodedAudioChunk::IsExposed(execution_context);
    case kEncodedVideoChunkTag:
      return V8EncodedVideoChunk::IsExposed(execution_context);
    case kMediaStreamTrack:
      return V8MediaStreamTrack::IsExposed(execution_context);
    case kCropTargetTag:
      return V8CropTarget::IsExposed(execution_context);
    case kRestrictionTargetTag:
      return V8RestrictionTarget::IsExposed(execution_context);
    case kMediaSourceHandleTag:
      return V8MediaSourceHandle::IsExposed(execution_context);
    default:
      return false;
  }
}

}  // namespace blink
