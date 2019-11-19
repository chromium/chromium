// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ELEMENTS_UPLOAD_DATA_STREAM_H_
#define NET_BASE_ELEMENTS_UPLOAD_DATA_STREAM_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/base/upload_data_stream.h"

namespace net {

class DrainableIOBuffer;
class IOBuffer;
class UploadElementReader;

// A non-chunked UploadDataStream consisting of one or more UploadElements.
class NET_EXPORT ElementsUploadDataStream : public UploadDataStream {
 public:
  ElementsUploadDataStream(
      std::vector<std::unique_ptr<UploadElementReader>> element_readers,
      int64_t identifier);

  ~ElementsUploadDataStream() override;

  // Creates an ElementsUploadDataStream with a single reader.  Returns a
  // std::unique_ptr<UploadDataStream> for ease of use.
  static std::unique_ptr<UploadDataStream> CreateWithReader(
      std::unique_ptr<UploadElementReader> reader,
      int64_t identifier);

 private:
  // UploadDataStream implementation.
  bool IsInMemory() const override;
  const std::vector<std::unique_ptr<UploadElementReader>>* GetElementReaders()
      const override;
  int InitInternal(const NetLogWithSource& net_log) override;
  int ReadInternal(IOBuffer* buf, int buf_len) override;
  void ResetInternal() override;

  // Runs Init() for all element readers.
  // This method is used to implement InitInternal().
  int InitElements(size_t start_index);

  // Called when the |index| element finishes initialization. If it succeeded,
  // continues with the |index + 1| element. Calls OnInitCompleted on error or
  // when all elements have been initialized.
  void OnInitElementCompleted(size_t index, int result);

  // Reads data from the element readers.
  // This method is used to implement Read().
  int ReadElements(const scoped_refptr<DrainableIOBuffer>& buf);

  // Resumes pending read and calls OnReadCompleted with a result when
  // necessary.
  void OnReadElementCompleted(const scoped_refptr<DrainableIOBuffer>& buf,
                              int result);

  // Processes result of UploadElementReader::Read(). If |result| indicates
  // success, updates |buf|'s offset. Otherwise, sets |read_failed_| to true.
  void ProcessReadResult(const scoped_refptr<DrainableIOBuffer>& buf,
                         int result);

  std::vector<std::unique_ptr<UploadElementReader>> element_readers_;

  // Index of the current upload element (i.e. the element currently being
  // read). The index is used as a cursor to iterate over elements in
  // |upload_data_|.
  size_t element_index_;

  // Set to actual error if read fails, otherwise set to net::OK.
  int read_error_;

  base::WeakPtrFactory<ElementsUploadDataStream> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ElementsUploadDataStream);
};

}  // namespace net

#endif  // NET_BASE_ELEMENTS_UPLOAD_DATA_STREAM_H_
