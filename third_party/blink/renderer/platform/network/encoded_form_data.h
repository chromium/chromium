/*
 * Copyright (C) 2004, 2006, 2008, 2011 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_NETWORK_ENCODED_FORM_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_NETWORK_ENCODED_FORM_DATA_H_

#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#include "services/network/public/mojom/data_pipe_getter.mojom-blink.h"

namespace blink {

class BlobDataHandle;

// Refcounted wrapper around a DataPipeGetter to allow sharing the move-only
// type. This is only needed so EncodedFormData/FormDataElement have a copy
// constructor.
class PLATFORM_EXPORT WrappedDataPipeGetter final
    : public RefCounted<WrappedDataPipeGetter> {
 public:
  explicit WrappedDataPipeGetter(
      network::mojom::blink::DataPipeGetterPtr data_pipe_getter)
      : data_pipe_getter_(std::move(data_pipe_getter)) {}
  ~WrappedDataPipeGetter() = default;

  network::mojom::blink::DataPipeGetterPtr* GetPtr() {
    return &data_pipe_getter_;
  }

 private:
  network::mojom::blink::DataPipeGetterPtr data_pipe_getter_;
};

class PLATFORM_EXPORT FormDataElement final {
  DISALLOW_NEW();

 public:
  FormDataElement() : type_(kData) {}
  explicit FormDataElement(const Vector<char>& array)
      : type_(kData), data_(array) {}
  FormDataElement(const String& filename,
                  long long file_start,
                  long long file_length,
                  double expected_file_modification_time)
      : type_(kEncodedFile),
        filename_(filename),
        file_start_(file_start),
        file_length_(file_length),
        expected_file_modification_time_(expected_file_modification_time) {}
  FormDataElement(const String& blob_uuid,
                  scoped_refptr<BlobDataHandle> optional_handle)
      : type_(kEncodedBlob),
        blob_uuid_(blob_uuid),
        optional_blob_data_handle_(std::move(optional_handle)) {}
  explicit FormDataElement(
      scoped_refptr<WrappedDataPipeGetter> data_pipe_getter)
      : type_(kDataPipe), data_pipe_getter_(std::move(data_pipe_getter)) {}

  bool IsSafeToSendToAnotherThread() const;

  enum Type { kData, kEncodedFile, kEncodedBlob, kDataPipe } type_;
  Vector<char> data_;
  String filename_;
  String blob_uuid_;
  scoped_refptr<BlobDataHandle> optional_blob_data_handle_;
  long long file_start_;
  long long file_length_;
  double expected_file_modification_time_;
  scoped_refptr<WrappedDataPipeGetter> data_pipe_getter_;
};

inline bool operator==(const FormDataElement& a, const FormDataElement& b) {
  if (&a == &b)
    return true;

  if (a.type_ != b.type_)
    return false;
  if (a.type_ == FormDataElement::kData)
    return a.data_ == b.data_;
  if (a.type_ == FormDataElement::kEncodedFile)
    return a.filename_ == b.filename_ && a.file_start_ == b.file_start_ &&
           a.file_length_ == b.file_length_ &&
           a.expected_file_modification_time_ ==
               b.expected_file_modification_time_;
  if (a.type_ == FormDataElement::kEncodedBlob)
    return a.blob_uuid_ == b.blob_uuid_;
  if (a.type_ == FormDataElement::kDataPipe)
    return a.data_pipe_getter_ == b.data_pipe_getter_;

  return true;
}

inline bool operator!=(const FormDataElement& a, const FormDataElement& b) {
  return !(a == b);
}

class PLATFORM_EXPORT EncodedFormData : public RefCounted<EncodedFormData> {
 public:
  enum EncodingType {
    kFormURLEncoded,    // for application/x-www-form-urlencoded
    kTextPlain,         // for text/plain
    kMultipartFormData  // for multipart/form-data
  };

  static scoped_refptr<EncodedFormData> Create();
  static scoped_refptr<EncodedFormData> Create(const void*, wtf_size_t);
  static scoped_refptr<EncodedFormData> Create(const CString&);
  static scoped_refptr<EncodedFormData> Create(const Vector<char>&);
  scoped_refptr<EncodedFormData> Copy() const;
  scoped_refptr<EncodedFormData> DeepCopy() const;
  ~EncodedFormData();

  void AppendData(const void* data, wtf_size_t);
  void AppendFile(const String& file_path);
  void AppendFileRange(const String& filename,
                       long long start,
                       long long length,
                       double expected_modification_time);
  void AppendBlob(const String& blob_uuid,
                  scoped_refptr<BlobDataHandle> optional_handle);
  void AppendDataPipe(scoped_refptr<WrappedDataPipeGetter> handle);

  void Flatten(Vector<char>&) const;  // omits files
  String FlattenToString() const;     // omits files

  bool IsEmpty() const { return elements_.IsEmpty(); }
  const Vector<FormDataElement>& Elements() const { return elements_; }
  Vector<FormDataElement>& MutableElements() { return elements_; }

  const Vector<char>& Boundary() const { return boundary_; }
  void SetBoundary(Vector<char> boundary) { boundary_ = boundary; }

  // Identifies a particular form submission instance.  A value of 0 is used
  // to indicate an unspecified identifier.
  void SetIdentifier(int64_t identifier) { identifier_ = identifier; }
  int64_t Identifier() const { return identifier_; }

  bool ContainsPasswordData() const { return contains_password_data_; }
  void SetContainsPasswordData(bool contains_password_data) {
    contains_password_data_ = contains_password_data;
  }

  static EncodingType ParseEncodingType(const String& type) {
    if (DeprecatedEqualIgnoringCase(type, "text/plain"))
      return kTextPlain;
    if (DeprecatedEqualIgnoringCase(type, "multipart/form-data"))
      return kMultipartFormData;
    return kFormURLEncoded;
  }

  // Size of the elements making up the EncodedFormData.
  unsigned long long SizeInBytes() const;

  bool IsSafeToSendToAnotherThread() const;

 private:
  EncodedFormData();
  EncodedFormData(const EncodedFormData&);

  Vector<FormDataElement> elements_;

  int64_t identifier_;
  Vector<char> boundary_;
  bool contains_password_data_;
};

inline bool operator==(const EncodedFormData& a, const EncodedFormData& b) {
  return a.Elements() == b.Elements();
}

inline bool operator!=(const EncodedFormData& a, const EncodedFormData& b) {
  return !(a == b);
}

}  // namespace blink

#endif
