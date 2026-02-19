// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_LOADER_DOCUMENT_LOADER_IMPL_H_
#define PDF_LOADER_DOCUMENT_LOADER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "pdf/loader/chunk_stream.h"
#include "pdf/loader/document_loader.h"

namespace chrome_pdf {

class DocumentLoaderImpl : public DocumentLoader {
 public:
  // Number was chosen in https://crbug.com/78264#c8
  static constexpr uint32_t kDefaultRequestSize = 65536;

  explicit DocumentLoaderImpl(Client* client);
  DocumentLoaderImpl(const DocumentLoaderImpl&) = delete;
  DocumentLoaderImpl& operator=(const DocumentLoaderImpl&) = delete;
  ~DocumentLoaderImpl() override;

  // DocumentLoader:
  bool Init(std::unique_ptr<URLLoaderWrapper> loader,
            const std::string& url) override;
  bool GetBlock(uint32_t position, base::span<uint8_t> buf) const override;
  bool IsDataAvailable(uint32_t position, uint32_t size) const override;
  void RequestData(uint32_t position, uint32_t size) override;
  bool IsDocumentComplete() const override;
  uint32_t GetDocumentSize() const override;
  uint32_t BytesReceived() const override;
  void ClearPendingRequests() override;

  // Exposed for unit tests.
  void SetPartialLoadingEnabled(bool enabled);
  bool is_partial_loader_active() const { return is_partial_loader_active_; }

 private:
  // Allow tests to access private Chunk class.
  friend class DocumentLoaderImplChunkTest;
  using DataStream = ChunkStream<kDefaultRequestSize>;

  // Chunk manages a single chunk buffer during PDF loading.
  // It encapsulates data accumulation and state transitions.
  class Chunk {
   public:
    Chunk();
    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;
    ~Chunk();

    // Resets the chunk to the initial state
    void Clear();

    // Set the chunk index (used when starting a new range request)
    void SetIndex(uint32_t index);

    // Append data to the chunk buffer
    // Returns the number of bytes actually appended
    size_t AppendData(base::span<const uint8_t> input);

    // Check if the chunk buffer is full (reached kChunkSize)
    bool IsFull() const;

    // Check if the chunk has no data
    bool IsEmpty() const;

    // Get the current chunk index
    uint32_t index() const { return chunk_index_; }

    // Compute the byte position at the end of current chunk data
    size_t EndPosition() const;

    // Transfer ownership of chunk data and advance to next chunk index
    // Resets data_size to 0 and increments chunk_index
    std::unique_ptr<DataStream::ChunkData> TakeDataAndAdvance();

   private:
    // Lazily allocate chunk_data if not already allocated
    void EnsureDataAllocated();

    uint32_t chunk_index_ = 0;
    size_t data_size_ = 0;
    std::unique_ptr<DataStream::ChunkData> chunk_data_;
  };

  // Called by the completion callback of the document's URLLoader.
  void DidOpenPartial(bool success);

  // Call to read data from the document's URLLoader.
  void ReadMore();

  // Called by the completion callback of the document's URLLoader.
  void DidRead(int32_t result);

  bool ShouldCancelLoading() const;
  void ContinueDownload();

  // Called when we complete server request.
  void ReadComplete();

  bool SaveBuffer(uint32_t input_size);
  void SaveChunkData();

  const raw_ptr<Client> client_;
  std::string url_;
  std::unique_ptr<URLLoaderWrapper> loader_;

  DataStream chunk_stream_;
  bool partial_loading_enabled_;  // Default determined by `kPdfPartialLoading`.
  bool is_partial_loader_active_ = false;

  std::vector<uint8_t> buffer_;

  // The current chunk DocumentLoader is working with.
  Chunk chunk_;

  // In units of Chunks.
  RangeSet pending_requests_;

  uint32_t bytes_received_ = 0;

  base::WeakPtrFactory<DocumentLoaderImpl> weak_factory_{this};
};

}  // namespace chrome_pdf

#endif  // PDF_LOADER_DOCUMENT_LOADER_IMPL_H_
