// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MHTML_HANDLE_WRITER_H_
#define CONTENT_RENDERER_MHTML_HANDLE_WRITER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/files/file.h"
#include "content/common/download/mhtml_file_writer.mojom-forward.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace base {
class TaskRunner;
}

namespace blink {
class WebThreadSafeData;
}

namespace mojo {
class SimpleWatcher;
}

namespace content {

// TODO(https://crbug.com/915966): This class needs unit tests.

// Handle wrapper for MHTML serialization to abstract the handle which data
// is written to. This is instantiated on the heap and is responsible for
// destroying itself after completing its write operation.
// Should only live in blocking sequenced threads.
class MHTMLHandleWriter {
 public:
  using MHTMLWriteCompleteCallback =
      base::OnceCallback<void(mojom::MhtmlSaveStatus)>;

  MHTMLHandleWriter(scoped_refptr<base::TaskRunner> main_thread_task_runner,
                    MHTMLWriteCompleteCallback callback);
  virtual ~MHTMLHandleWriter();

  void WriteContents(std::vector<blink::WebThreadSafeData> mhtml_contents);

  // Finalizes the writing operation, recording the UMA, closing the handle,
  // and deleting itself.
  void Finish(mojom::MhtmlSaveStatus save_status);

 protected:
  virtual void WriteContentsImpl(
      std::vector<blink::WebThreadSafeData> mhtml_contents) = 0;

  virtual void Close() = 0;

 private:
  base::TimeTicks mhtml_write_start_time_;

  scoped_refptr<base::TaskRunner> main_thread_task_runner_;
  MHTMLWriteCompleteCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(MHTMLHandleWriter);
};

// Wraps a base::File target to write MHTML contents to.
// This implementation immediately finishes after writing all MHTML contents
// to the file handle.
class MHTMLFileHandleWriter : public MHTMLHandleWriter {
 public:
  MHTMLFileHandleWriter(scoped_refptr<base::TaskRunner> main_thread_task_runner,
                        MHTMLWriteCompleteCallback callback,
                        base::File file);
  ~MHTMLFileHandleWriter() override;

 protected:
  // Writes the serialized and encoded MHTML data from WebThreadSafeData
  // instances directly to the file handle passed from the Browser.
  void WriteContentsImpl(
      std::vector<blink::WebThreadSafeData> mhtml_contents) override;

  void Close() override;

 private:
  base::File file_;

  DISALLOW_COPY_AND_ASSIGN(MHTMLFileHandleWriter);
};

// Wraps a mojo::ScopedDataPipeProducerHandle target to write MHTML contents to.
// This implementation does not immediately finish and destroy itself due to
// the limited size of the data pipe buffer. We must ensure all data is
// written to the handle before finishing the write operation.
class MHTMLProducerHandleWriter : public MHTMLHandleWriter {
 public:
  MHTMLProducerHandleWriter(
      scoped_refptr<base::TaskRunner> main_thread_task_runner,
      MHTMLWriteCompleteCallback callback,
      mojo::ScopedDataPipeProducerHandle producer);
  ~MHTMLProducerHandleWriter() override;

 protected:
  // Creates a new SequencedTaskRunner to dispatch |watcher_| invocations on.
  void WriteContentsImpl(
      std::vector<blink::WebThreadSafeData> mhtml_contents) override;

  void Close() override;

 private:
  void BeginWatchingHandle();

  // Writes the serialized and encoded MHTML data from WebThreadSafeData
  // instances to producer while possible.
  void TryWritingContents(MojoResult result,
                          const mojo::HandleSignalsState& state);

  mojo::ScopedDataPipeProducerHandle producer_;

  std::vector<blink::WebThreadSafeData> mhtml_contents_;
  std::unique_ptr<mojo::SimpleWatcher> watcher_;

  size_t current_block_;
  size_t write_position_;

  DISALLOW_COPY_AND_ASSIGN(MHTMLProducerHandleWriter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MHTML_HANDLE_WRITER_H_