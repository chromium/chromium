// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/modules/v8/serialization/v8_script_value_serializer_for_modules.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_crypto.h"
#include "third_party/blink/public/platform/web_crypto_key.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/v8_script_value_serializer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_rect_read_only.h"
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
#include "third_party/blink/renderer/bindings/modules/v8/v8_landmark.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_source_handle.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_track.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_point_2d.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_restriction_target.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_certificate.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_data_channel.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_audio_frame.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_video_frame.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/crypto/crypto_key.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_directory_handle.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_file_handle.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_handle.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system.h"
#include "third_party/blink/renderer/modules/mediasource/media_source_attachment_supplement.h"
#include "third_party/blink/renderer/modules/mediasource/media_source_handle_attachment.h"
#include "third_party/blink/renderer/modules/mediasource/media_source_handle_impl.h"
#include "third_party/blink/renderer/modules/mediasource/media_source_handle_transfer_list.h"
#include "third_party/blink/renderer/modules/mediastream/crop_target.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_utils.h"
#include "third_party/blink/renderer/modules/mediastream/restriction_target.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_certificate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_data_channel.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_data_channel_attachment.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_data_channel_transfer_list.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame_delegate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame_delegate.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_data.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_data_attachment.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_data_transfer_list.h"
#include "third_party/blink/renderer/modules/webcodecs/decoder_buffer_attachment.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_audio_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame_attachment.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame_transfer_list.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

// static
bool V8ScriptValueSerializerForModules::ExtractTransferable(
    v8::Isolate* isolate,
    v8::Local<v8::Value> object,
    wtf_size_t object_index,
    Transferables& transferables,
    ExceptionState& exception_state) {
  // Give the core/ implementation a chance to try first.
  // If it didn't recognize the kind of object, try the modules types.
  if (V8ScriptValueSerializer::ExtractTransferable(
          isolate, object, object_index, transferables, exception_state)) {
    return true;
  }
  if (exception_state.HadException())
    return false;

  if (VideoFrame* video_frame = V8VideoFrame::ToWrappable(isolate, object)) {
    VideoFrameTransferList* transfer_list =
        transferables.GetOrCreateTransferList<VideoFrameTransferList>();
    if (transfer_list->video_frames.Contains(video_frame)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "VideoFrame at index " + String::Number(object_index) +
              " is a duplicate of an earlier VideoFrame.");
      return false;
    }
    transfer_list->video_frames.push_back(video_frame);
    return true;
  }

  if (AudioData* audio_data = V8AudioData::ToWrappable(isolate, object)) {
    AudioDataTransferList* transfer_list =
        transferables.GetOrCreateTransferList<AudioDataTransferList>();
    if (transfer_list->audio_data_collection.Contains(audio_data)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "AudioData at index " + String::Number(object_index) +
              " is a duplicate of an earlier AudioData.");
      return false;
    }
    transfer_list->audio_data_collection.push_back(audio_data);
    return true;
  }

  if (RTCDataChannel* channel =
          V8RTCDataChannel::ToWrappable(isolate, object)) {
    if (RuntimeEnabledFeatures::TransferableRTCDataChannelEnabled(
            CurrentExecutionContext(isolate))) {
      RTCDataChannelTransferList* transfer_list =
          transferables.GetOrCreateTransferList<RTCDataChannelTransferList>();

      if (transfer_list->data_channel_collection.Contains(channel)) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kDataCloneError,
            "RTCDataChannel at index " + String::Number(object_index) +
                " is a duplicate of an earlier RTCDataChannel.");
        return false;
      }

      transfer_list->data_channel_collection.push_back(channel);
      return true;
    }
  }

  if (MediaStreamTrack* track =
          V8MediaStreamTrack::ToWrappable(isolate, object)) {
    if (RuntimeEnabledFeatures::MediaStreamTrackTransferEnabled(
            CurrentExecutionContext(isolate))) {
      if (transferables.media_stream_tracks.Contains(track)) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kDataCloneError,
            "MediaStreamTrack at index " + String::Number(object_index) +
                " is a duplicate of an earlier MediaStreamTrack.");
        return false;
      }
      transferables.media_stream_tracks.push_back(track);
      return true;
    }
  }

  if (MediaSourceHandleImpl* media_source_handle =
          V8MediaSourceHandle::ToWrappable(isolate, object)) {
    MediaSourceHandleTransferList* transfer_list =
        transferables.GetOrCreateTransferList<MediaSourceHandleTransferList>();
    if (transfer_list->media_source_handles.Contains(media_source_handle)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "MediaSourceHandle at index " + String::Number(object_index) +
              " is a duplicate of an earlier MediaSourceHandle.");
      return false;
    }
    if (media_source_handle->is_detached()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "MediaSourceHandle at index " + String::Number(object_index) +
              " is detached and cannot be transferred.");
      return false;
    }
    if (media_source_handle->is_used()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "MediaSourceHandle at index " + String::Number(object_index) +
              " has been used as srcObject of media element already, and "
              "cannot be transferred.");
      return false;
    }
    transfer_list->media_source_handles.push_back(media_source_handle);
    return true;
  }

  return false;
}

bool V8ScriptValueSerializerForModules::WriteDOMObject(
    ScriptWrappable* wrappable,
    ExceptionState& exception_state) {
  // Give the core/ implementation a chance to try first.
  // If it didn't recognize the kind of wrapper, try the modules types.
  if (V8ScriptValueSerializer::WriteDOMObject(wrappable, exception_state))
    return true;
  if (exception_state.HadException())
    return false;

  ScriptWrappable::TypeDispatcher dispatcher(wrappable);
  if (auto* crypto_key = dispatcher.ToMostDerived<CryptoKey>()) {
    return WriteCryptoKey(crypto_key->Key(), exception_state);
  }
  if (auto* fs = dispatcher.ToMostDerived<DOMFileSystem>()) {
    if (!fs->Clonable()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "A FileSystem object could not be cloned.");
      return false;
    }
    WriteAndRequireInterfaceTag(kDOMFileSystemTag);
    // This locks in the values of the FileSystemType enumerators.
    WriteUint32(static_cast<uint32_t>(fs->GetType()));
    WriteUTF8String(fs->name());
    WriteUTF8String(fs->RootURL().GetString());
    return true;
  }
  if (auto* file_handle = dispatcher.ToMostDerived<FileSystemFileHandle>()) {
    if (!RuntimeEnabledFeatures::FileSystemAccessEnabled(
            ExecutionContext::From(GetScriptState()))) {
      return false;
    }
    return WriteFileSystemHandle(kFileSystemFileHandleTag, file_handle);
  }
  if (auto* dir_handle =
          dispatcher.ToMostDerived<FileSystemDirectoryHandle>()) {
    if (!RuntimeEnabledFeatures::FileSystemAccessEnabled(
            ExecutionContext::From(GetScriptState()))) {
      return false;
    }
    return WriteFileSystemHandle(kFileSystemDirectoryHandleTag, dir_handle);
  }
  if (auto* certificate = dispatcher.ToMostDerived<RTCCertificate>()) {
    rtc::RTCCertificatePEM pem = certificate->Certificate()->ToPEM();
    WriteAndRequireInterfaceTag(kRTCCertificateTag);
    WriteUTF8String(pem.private_key().c_str());
    WriteUTF8String(pem.certificate().c_str());
    return true;
  }
  if (auto* audio_frame = dispatcher.ToMostDerived<RTCEncodedAudioFrame>()) {
    if (IsForStorage()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                        "An RTCEncodedAudioFrame cannot be "
                                        "serialized for storage.");
      return false;
    }
    return WriteRTCEncodedAudioFrame(audio_frame);
  }
  if (auto* video_frame = dispatcher.ToMostDerived<RTCEncodedVideoFrame>()) {
    if (IsForStorage()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                        "An RTCEncodedVideoFrame cannot be "
                                        "serialized for storage.");
      return false;
    }
    return WriteRTCEncodedVideoFrame(video_frame);
  }
  if (auto* video_frame = dispatcher.ToMostDerived<VideoFrame>()) {
    if (IsForStorage()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                        "A VideoFrame cannot be serialized for "
                                        "storage.");
      return false;
    }
    scoped_refptr<VideoFrameHandle> handle = video_frame->handle()->Clone();
    if (!handle) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                        "A VideoFrame could not be cloned "
                                        "because it was closed.");
      return false;
    }
    return WriteVideoFrameHandle(std::move(handle));
  }
  if (auto* audio_data = dispatcher.ToMostDerived<AudioData>()) {
    if (IsForStorage()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                        "AudioData cannot be serialized for "
                                        "storage.");
      return false;
    }
    scoped_refptr<media::AudioBuffer> data = audio_data->data();
    if (!data) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                        "AudioData could not be cloned "
                                        "because it was closed.");
      return false;
    }
    return WriteMediaAudioBuffer(std::move(data));
  }
  if (auto* audio_chunk = dispatcher.ToMostDerived<EncodedAudioChunk>()) {
    if (IsForStorage()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "Encoded chunks cannot be serialized for storage.");
      return false;
    }
    return WriteDecoderBuffer(audio_chunk->buffer(), /*for_audio=*/true);
  }
  if (auto* video_chunk = dispatcher.ToMostDerived<EncodedVideoChunk>()) {
    if (IsForStorage()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "Encoded chunks cannot be serialized for storage.");
      return false;
    }
    return WriteDecoderBuffer(video_chunk->buffer(), /*for_audio=*/false);
  }
  if (auto* track = dispatcher.DowncastTo<MediaStreamTrack>()) {
    if (!RuntimeEnabledFeatures::MediaStreamTrackTransferEnabled(
            ExecutionContext::From(GetScriptState()))) {
      return false;
    }
    if (IsForStorage()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "A MediaStreamTrack cannot be serialized for storage.");
      return false;
    }
    return WriteMediaStreamTrack(track, dispatcher, exception_state);
  }
  if (auto* channel = dispatcher.DowncastTo<RTCDataChannel>()) {
    if (!RuntimeEnabledFeatures::TransferableRTCDataChannelEnabled(
            ExecutionContext::From(GetScriptState()))) {
      return false;
    }
    if (IsForStorage()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "An RTCDataChannel cannot be serialized for storage.");
      return false;
    }
    if (!channel->IsTransferable()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "RTCDataChannel at index is no longer transferable. Transfers must "
          "occur on creation, and before any calls to send().");
      return false;
    }
    return WriteRTCDataChannel(channel);
  }
  if (auto* crop_target = dispatcher.ToMostDerived<CropTarget>()) {
    if (!RuntimeEnabledFeatures::RegionCaptureEnabled(
            ExecutionContext::From(GetScriptState()))) {
      return false;
    }
    if (IsForStorage()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "A CropTarget cannot be serialized for storage.");
      return false;
    }
    return WriteCropTarget(crop_target);
  }
  if (auto* restriction_target =
          dispatcher.ToMostDerived<RestrictionTarget>()) {
    if (!RuntimeEnabledFeatures::ElementCaptureEnabled(
            ExecutionContext::From(GetScriptState()))) {
      return false;
    }
    if (IsForStorage()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "A RestrictionTarget cannot be serialized for storage.");
      return false;
    }
    return WriteRestrictionTarget(restriction_target);
  }
  if (auto* media_source_handle =
          dispatcher.ToMostDerived<MediaSourceHandleImpl>()) {
    if (IsForStorage()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "A MediaSourceHandle cannot be serialized for storage.");
      return false;
    }
    if (const Transferables* transferables = GetTransferables()) {
      if (const MediaSourceHandleTransferList* transfer_list =
              transferables
                  ->GetTransferListIfExists<MediaSourceHandleTransferList>()) {
        if (transfer_list->media_source_handles.Find(media_source_handle) !=
            kNotFound) {
          return WriteMediaSourceHandle(media_source_handle, exception_state);
        }
      }
    }
    exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                      "A MediaSourceHandle could not be cloned "
                                      "because it was not transferred.");
    return false;
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
    case kWebCryptoAlgorithmIdEd25519:
      return kEd25519Tag;
    case kWebCryptoAlgorithmIdX25519:
      return kX25519Tag;
  }
  NOTREACHED_IN_MIGRATION() << "Unknown algorithm ID " << id;
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
  NOTREACHED_IN_MIGRATION() << "Unknown asymmetric key type " << key_type;
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
  NOTREACHED_IN_MIGRATION() << "Unknown named curve " << named_curve;
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
  WriteAndRequireInterfaceTag(kCryptoKeyTag);

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
      WriteRawBytes(params.PublicExponent().data(),
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
      switch (algorithm.Id()) {
        case kWebCryptoAlgorithmIdEd25519:
        case kWebCryptoAlgorithmIdX25519: {
          CryptoKeySubTag tag = algorithm.Id() == kWebCryptoAlgorithmIdEd25519
                                    ? kEd25519KeyTag
                                    : kX25519KeyTag;
          WriteOneByte(tag);
          WriteUint32(AlgorithmIdForWireFormat(algorithm.Id()));
          WriteUint32(AsymmetricKeyTypeForWireFormat(key.GetType()));
          break;
        }
        default:
          DCHECK(WebCryptoAlgorithm::IsKdf(algorithm.Id()));
          WriteOneByte(kNoParamsKeyTag);
          WriteUint32(AlgorithmIdForWireFormat(algorithm.Id()));
      }
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
  WriteRawBytes(key_data.data(), key_data.size());

  return true;
}

bool V8ScriptValueSerializerForModules::WriteFileSystemHandle(
    SerializationTag tag,
    FileSystemHandle* file_system_handle) {
  mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken> token =
      file_system_handle->Transfer();

  SerializedScriptValue::FileSystemAccessTokensArray& tokens_array =
      GetSerializedScriptValue()->FileSystemAccessTokens();

  tokens_array.push_back(std::move(token));
  const uint32_t token_index = static_cast<uint32_t>(tokens_array.size() - 1);

  WriteAndRequireInterfaceTag(tag);
  WriteUTF8String(file_system_handle->name());
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

  WriteAndRequireInterfaceTag(kRTCEncodedAudioFrameTag);
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

  WriteAndRequireInterfaceTag(kRTCEncodedVideoFrameTag);
  WriteUint32(index);
  return true;
}

bool V8ScriptValueSerializerForModules::WriteVideoFrameHandle(
    scoped_refptr<VideoFrameHandle> handle) {
  auto* attachment =
      GetSerializedScriptValue()->GetOrCreateAttachment<VideoFrameAttachment>();
  auto& frames = attachment->Handles();
  frames.push_back(std::move(handle));
  const uint32_t index = static_cast<uint32_t>(frames.size() - 1);

  WriteAndRequireInterfaceTag(kVideoFrameTag);
  WriteUint32(index);

  return true;
}

bool V8ScriptValueSerializerForModules::WriteMediaAudioBuffer(
    scoped_refptr<media::AudioBuffer> audio_data) {
  auto* attachment =
      GetSerializedScriptValue()->GetOrCreateAttachment<AudioDataAttachment>();
  auto& audio_buffers = attachment->AudioBuffers();
  audio_buffers.push_back(std::move(audio_data));
  const uint32_t index = static_cast<uint32_t>(audio_buffers.size() - 1);

  WriteAndRequireInterfaceTag(kAudioDataTag);
  WriteUint32(index);

  return true;
}

bool V8ScriptValueSerializerForModules::WriteDecoderBuffer(
    scoped_refptr<media::DecoderBuffer> data,
    bool for_audio) {
  auto* attachment = GetSerializedScriptValue()
                         ->GetOrCreateAttachment<DecoderBufferAttachment>();
  auto& buffers = attachment->Buffers();
  buffers.push_back(std::move(data));
  const uint32_t index = static_cast<uint32_t>(buffers.size() - 1);

  WriteAndRequireInterfaceTag(for_audio ? kEncodedAudioChunkTag
                                        : kEncodedVideoChunkTag);
  WriteUint32(index);

  return true;
}

bool V8ScriptValueSerializerForModules::WriteMediaStreamTrack(
    MediaStreamTrack* track,
    ScriptWrappable::TypeDispatcher& dispatcher,
    ExceptionState& exception_state) {
  String message;
  if (!track->TransferAllowed(message)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                      message);
    return false;
  }
  std::optional<const MediaStreamDevice> device = track->device();
  // TODO(crbug.com/1352414): Replace this UnguessableToken with a mojo
  // interface.
  auto transfer_id = base::UnguessableToken::Create();

  WriteAndRequireInterfaceTag(kMediaStreamTrack);
  auto track_impl_subtype = SerializeTrackImplSubtype(dispatcher);
  WriteUint32Enum(track_impl_subtype);
  WriteUnguessableToken(*device->serializable_session_id());
  WriteUnguessableToken(transfer_id);
  WriteUTF8String(track->kind());
  WriteUTF8String(track->id());
  WriteUTF8String(track->label());
  WriteOneByte(track->enabled());
  WriteOneByte(track->muted());
  WriteUint32Enum(SerializeContentHint(track->Component()->ContentHint()));
  WriteUint32Enum(SerializeReadyState(track->Component()->GetReadyState()));
  // Using `switch` to ensure new enum values are handled.
  switch (track_impl_subtype) {
    case SerializedTrackImplSubtype::kTrackImplSubtypeBase:
      // No additional data needs to be serialized.
      break;
    case SerializedTrackImplSubtype::kTrackImplSubtypeCanvasCapture:
    case SerializedTrackImplSubtype::kTrackImplSubtypeGenerator:
      NOTREACHED_IN_MIGRATION()
          << "device type is " << device->type << " but track impl subtype is "
          << static_cast<uint32_t>(track_impl_subtype);
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "MediaStreamTrack could not be serialized.");
      return false;
    case SerializedTrackImplSubtype::kTrackImplSubtypeBrowserCapture:
      MediaStreamSource* const source = track->Component()->Source();
      DCHECK(source);
      DCHECK_EQ(source->GetType(), MediaStreamSource::kTypeVideo);
      MediaStreamVideoSource* const native_source =
          MediaStreamVideoSource::GetVideoSource(source);
      DCHECK(native_source);
      WriteUint32(native_source->GetSubCaptureTargetVersion());
      break;
  }
  // TODO(crbug.com/1288839): Needs to move to FinalizeTransfer?
  track->BeingTransferred(transfer_id);
  return true;
}

bool V8ScriptValueSerializerForModules::WriteRTCDataChannel(
    RTCDataChannel* channel) {
  if (!RuntimeEnabledFeatures::TransferableRTCDataChannelEnabled()) {
    return false;
  }

  auto* attachment = GetSerializedScriptValue()
                         ->GetOrCreateAttachment<RTCDataChannelAttachment>();
  using NativeDataChannelVector =
      Vector<rtc::scoped_refptr<webrtc::DataChannelInterface>>;
  NativeDataChannelVector& channels = attachment->DataChannels();
  channels.push_back(channel->TransferUnderlyingChannel());
  const uint32_t index = static_cast<uint32_t>(channels.size() - 1);

  WriteAndRequireInterfaceTag(kRTCDataChannel);
  WriteUint32(index);

  return true;
}

bool V8ScriptValueSerializerForModules::WriteCropTarget(
    CropTarget* crop_target) {
  CHECK(crop_target);
  const String& id = crop_target->GetId();
  CHECK(!id.empty());
  WriteAndRequireInterfaceTag(kCropTargetTag);
  WriteUTF8String(id);
  return true;
}

bool V8ScriptValueSerializerForModules::WriteRestrictionTarget(
    RestrictionTarget* restriction_target) {
  CHECK(restriction_target);
  const String& id = restriction_target->GetId();
  CHECK(!id.empty());
  WriteAndRequireInterfaceTag(kRestrictionTargetTag);
  WriteUTF8String(id);
  return true;
}

bool V8ScriptValueSerializerForModules::WriteMediaSourceHandle(
    MediaSourceHandleImpl* handle,
    ExceptionState& exception_state) {
  if (handle->is_serialized()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataCloneError,
        "MediaSourceHandle is already serialized.");
    return false;
  }

  if (handle->is_used()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                      "MediaSourceHandle has been used as "
                                      "srcObject of media element already.");
    return false;
  }

  // The collection of handle-attachment involved in serialization.
  auto* attachment = GetSerializedScriptValue()
                         ->GetOrCreateAttachment<MediaSourceHandleAttachment>();

  // The collection of underlying scoped_refptr<MediaSourceAttachmentProvider>
  // and internal object URLs involved in serialization. Each is the internal
  // state of a MediaSourceHandleImpl. Add the internal state of |handle| to it
  // and serialize it using the index of that state in the vector.
  auto& attachments = attachment->Attachments();

  scoped_refptr<HandleAttachmentProvider> media_source_attachment_provider =
      handle->TakeAttachmentProvider();
  // The two handle checks, above, (!is_serialized() and !is_used()) should
  // prevent us from ever having a missing |media_source_attachment_provider|
  // here.
  DCHECK(media_source_attachment_provider);

  attachments.push_back(MediaSourceHandleAttachment::HandleInternals{
      .attachment_provider = std::move(media_source_attachment_provider),
      .internal_blob_url = handle->GetInternalBlobURL()});
  handle->mark_serialized();
  const uint32_t index = static_cast<uint32_t>(attachments.size() - 1);

  WriteAndRequireInterfaceTag(kMediaSourceHandleTag);
  WriteUint32(index);

  return true;
}

}  // namespace blink
