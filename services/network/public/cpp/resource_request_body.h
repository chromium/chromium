// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_REQUEST_BODY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_REQUEST_BODY_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom-shared.h"
#include "services/network/public/mojom/data_pipe_getter.mojom-shared.h"
#include "services/network/public/mojom/url_request.mojom-shared.h"

namespace base {
class Time;
}

namespace blink {
namespace mojom {
class FetchAPIRequestBodyDataView;
}  // namespace mojom
}  // namespace blink

namespace network {

// ResourceRequestBody represents body (i.e. upload data) of a HTTP request.
class COMPONENT_EXPORT(NETWORK_CPP_BASE) ResourceRequestBody
    : public base::RefCountedThreadSafe<ResourceRequestBody> {
 public:
  using ReadOnlyOnce = DataElementChunkedDataPipe::ReadOnlyOnce;

  ResourceRequestBody();

  ResourceRequestBody(const ResourceRequestBody&) = delete;
  ResourceRequestBody& operator=(const ResourceRequestBody&) = delete;

  // Creates ResourceRequestBody that holds a copy of |bytes|.
  static scoped_refptr<ResourceRequestBody> CreateFromBytes(const char* bytes,
                                                            size_t length);

  void AppendBytes(std::vector<uint8_t> bytes);
  void AppendBytes(const char* bytes, int bytes_len);
  void AppendFileRange(const base::FilePath& file_path,
                       uint64_t offset,
                       uint64_t length,
                       const base::Time& expected_modification_time);

  void AppendDataPipe(
      mojo::PendingRemote<mojom::DataPipeGetter> data_pipe_getter);

  // |chunked_data_pipe_getter| will provide the upload body for a chunked
  // upload. Unlike the other methods, which support concatenating data of
  // various types, when this is called, |chunked_data_pipe_getter| will provide
  // the entire response body, and other types of data may not added when
  // sending chunked data.
  //
  // It's unclear how widespread support for chunked uploads is, since there are
  // no web APIs that send uploads with unknown request body sizes, so this
  // method should only be used when talking to servers that are are known to
  // support chunked uploads.
  void SetToChunkedDataPipe(mojo::PendingRemote<mojom::ChunkedDataPipeGetter>
                                chunked_data_pipe_getter,
                            ReadOnlyOnce read_only_once);
  // Almost same as above except |chunked_data_pipe_getter| is read only once
  // and you must talk with a server supporting chunked upload.
  void SetToReadOnceStream(mojo::PendingRemote<mojom::ChunkedDataPipeGetter>
                               chunked_data_pipe_getter);
  void SetAllowHTTP1ForStreamingUpload(bool allow) {
    allow_http1_for_streaming_upload_ = allow;
  }
  bool AllowHTTP1ForStreamingUpload() const {
    return allow_http1_for_streaming_upload_;
  }

  const std::vector<DataElement>* elements() const { return &elements_; }
  std::vector<DataElement>* elements_mutable() { return &elements_; }
  void swap_elements(std::vector<DataElement>* elements) {
    elements_.swap(*elements);
  }

  // Identifies a particular upload instance, which is used by the cache to
  // formulate a cache key.  This value should be unique across browser
  // sessions.  A value of 0 is used to indicate an unspecified identifier.
  void set_identifier(int64_t id) { identifier_ = id; }
  int64_t identifier() const { return identifier_; }

  // Returns paths referred to by |elements| of type
  // network::mojom::DataElementDataView::Tag::kFile.
  std::vector<base::FilePath> GetReferencedFiles() const;

  // Sets the flag which indicates whether the post data contains sensitive
  // information like passwords.
  void set_contains_sensitive_info(bool contains_sensitive_info) {
    contains_sensitive_info_ = contains_sensitive_info;
  }
  bool contains_sensitive_info() const { return contains_sensitive_info_; }

 private:
  friend class base::RefCountedThreadSafe<ResourceRequestBody>;
  friend struct mojo::StructTraits<blink::mojom::FetchAPIRequestBodyDataView,
                                   scoped_refptr<network::ResourceRequestBody>>;
  friend struct mojo::StructTraits<network::mojom::URLRequestBodyDataView,
                                   scoped_refptr<network::ResourceRequestBody>>;
  ~ResourceRequestBody();

  bool EnableToAppendElement() const;

  std::vector<DataElement> elements_;
  int64_t identifier_;

  bool contains_sensitive_info_;

  bool allow_http1_for_streaming_upload_ = false;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_REQUEST_BODY_H_
