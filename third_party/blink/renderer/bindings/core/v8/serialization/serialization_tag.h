// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SERIALIZATION_SERIALIZATION_TAG_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SERIALIZATION_SERIALIZATION_TAG_H_

namespace blink {

// Serialization format is a sequence of tags followed by zero or more data
// arguments.  Tags always take exactly one byte. A serialized stream first
// begins with a complete VersionTag. If the stream does not begin with a
// VersionTag, we assume that the stream is in format 0.

// Tags which are not interpreted by Blink (but instead by V8) are omitted here.

// This format is private to the implementation of SerializedScriptValue. Do not
// rely on it externally. It is safe to persist a SerializedScriptValue as a
// binary blob, but this code should always be used to interpret it.

// WebCoreStrings are read as (length:uint32_t, string:UTF8[length]).
// RawStrings are read as (length:uint32_t, string:UTF8[length]).
// RawUCharStrings are read as
//     (length:uint32_t, string:UChar[length/sizeof(UChar)]).
// RawFiles are read as
//     (path:WebCoreString, url:WebCoreStrng, type:WebCoreString).
// There is a reference table that maps object references (uint32_t) to
// v8::Values.
// Tokens marked with (ref) are inserted into the reference table and given the
// next object reference ID after decoding.
// All tags except InvalidTag, PaddingTag, ReferenceCountTag, VersionTag,
// GenerateFreshObjectTag and GenerateFreshArrayTag push their results to the
// deserialization stack.
// There is also an 'open' stack that is used to resolve circular references.
// Objects or arrays may contain self-references. Before we begin to deserialize
// the contents of these values, they are first given object reference IDs (by
// GenerateFreshObjectTag/GenerateFreshArrayTag); these reference IDs are then
// used with ObjectReferenceTag to tie the recursive knot.
enum SerializationTag {
  kMessagePortTag = 'M',  // index:int -> MessagePort. Fills the result with
                          // transferred MessagePort.
  kMojoHandleTag = 'h',   // index:int -> MojoHandle. Fills the result with
                          // transferred MojoHandle.
  kBlobTag = 'b',  // uuid:WebCoreString, type:WebCoreString, size:uint64_t ->
                   // Blob (ref)
  kBlobIndexTag = 'i',      // index:int32_t -> Blob (ref)
  kFileTag = 'f',           // file:RawFile -> File (ref)
  kFileIndexTag = 'e',      // index:int32_t -> File (ref)
  kDOMFileSystemTag = 'd',  // type:int32_t, name:WebCoreString,
                            // uuid:WebCoreString -> FileSystem (ref)
  kNativeFileSystemFileHandleTag = 'n',  // name:WebCoreString, index:uint32_t
                                         // -> NativeFileSystemFileHandle (ref)
  kNativeFileSystemDirectoryHandleTag =
      'N',  // name:WebCoreString, index:uint32_t ->
            // NativeFileSystemDirectoryHandle (ref)
  kFileListTag =
      'l',  // length:uint32_t, files:RawFile[length] -> FileList (ref)
  kFileListIndexTag =
      'L',  // length:uint32_t, files:int32_t[length] -> FileList (ref)
  kImageDataTag = '#',    // tags terminated by ImageSerializationTag::kEnd (see
                          // SerializedColorParams.h), width:uint32_t,
                          // height:uint32_t, pixelDataLength:uint64_t,
                          // data:byte[pixelDataLength]
                          // -> ImageData (ref)
  kImageBitmapTag = 'g',  // tags terminated by ImageSerializationTag::kEnd (see
                          // SerializedColorParams.h), width:uint32_t,
                          // height:uint32_t, pixelDataLength:uint32_t,
                          // data:byte[pixelDataLength]
                          // -> ImageBitmap (ref)
  kImageBitmapTransferTag =
      'G',  // index:uint32_t -> ImageBitmap. For ImageBitmap transfer
  kOffscreenCanvasTransferTag = 'H',  // index, width, height, id,
                                      // filter_quality::uint32_t ->
                                      // OffscreenCanvas. For OffscreenCanvas
                                      // transfer
  kReadableStreamTransferTag = 'r',   // index:uint32_t
  kTransformStreamTransferTag = 'm',  // index:uint32_t
  kWritableStreamTransferTag = 'w',   // index:uint32_t
  kDOMPointTag = 'Q',                 // x:Double, y:Double, z:Double, w:Double
  kDOMPointReadOnlyTag = 'W',         // x:Double, y:Double, z:Double, w:Double
  kDOMRectTag = 'E',          // x:Double, y:Double, width:Double, height:Double
  kDOMRectReadOnlyTag = 'R',  // x:Double, y:Double, width:Double, height:Double
  kDOMQuadTag = 'T',          // p1:Double, p2:Double, p3:Double, p4:Double
  kDOMMatrixTag = 'Y',        // m11..m44: 16 Double
  kDOMMatrixReadOnlyTag = 'U',    // m11..m44: 16 Double
  kDOMMatrix2DTag = 'I',          // a..f: 6 Double
  kDOMMatrix2DReadOnlyTag = 'O',  // a..f: 6 Double
  kCryptoKeyTag = 'K',            // subtag:byte, props, usages:uint32_t,
                        // keyDataLength:uint32_t, keyData:byte[keyDataLength]
  //                 If subtag=AesKeyTag:
  //                     props = keyLengthBytes:uint32_t, algorithmId:uint32_t
  //                 If subtag=HmacKeyTag:
  //                     props = keyLengthBytes:uint32_t, hashId:uint32_t
  //                 If subtag=RsaHashedKeyTag:
  //                     props = algorithmId:uint32_t, type:uint32_t,
  //                     modulusLengthBits:uint32_t,
  //                     publicExponentLength:uint32_t,
  //                     publicExponent:byte[publicExponentLength],
  //                     hashId:uint32_t
  //                 If subtag=EcKeyTag:
  //                     props = algorithmId:uint32_t, type:uint32_t,
  //                     namedCurve:uint32_t
  kRTCCertificateTag = 'k',  // length:uint32_t, pemPrivateKey:WebCoreString,
                             // pemCertificate:WebCoreString
  kDetectedBarcodeTag =
      'B',  // raw_value:WebCoreString, bounding_box:DOMRectReadOnly,
            // format:String, corner_points:Point2D[length] ->
            // DetectedBarcode (ref)
  kDetectedFaceTag =
      'F',  // raw_value:WebCoreString, bounding_box:DOMRectReadOnly,
            // corner_points:Point2D[length] -> DetectedText (ref)
  kDetectedTextTag = 't',  // bounding_box:DOMRectReadOnly,
                           // landmarks:Landmark[length] -> DetectedFace (ref)
  kDOMExceptionTag = 'x',  // name:String,message:String,stack:String
  kVersionTag = 0xFF       // version:uint32_t -> Uses this as the file version.
};

}  // namespace blink

#endif
