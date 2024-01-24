// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_IO_CONTEXT_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_IO_CONTEXT_H_

#include <map>

#include "base/functional/callback.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {

class DevToolsIOContext final {
 public:
  class Stream : public base::RefCountedDeleteOnSequence<Stream> {
   public:
    enum Status {
      StatusSuccess,
      StatusEOF,
      StatusFailure
    };

    using ReadCallback =
        base::OnceCallback<void(std::unique_ptr<std::string> data,
                                bool base64_encoded,
                                int status)>;

    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;

    virtual bool SupportsSeek() const;
    virtual void Read(off_t position,
                      size_t max_size,
                      ReadCallback callback) = 0;

   protected:
    friend class base::DeleteHelper<content::DevToolsIOContext::Stream>;
    friend class base::RefCountedDeleteOnSequence<Stream>;

    explicit Stream(scoped_refptr<base::SequencedTaskRunner> task_runner);
    virtual ~Stream() = 0;

    // Sub-class API:

    // Caller is reposnsible for generating a unique handle.
    void Register(DevToolsIOContext* context, const std::string& handle);
    // We generate handle for the caller and return it.
    std::string Register(DevToolsIOContext* context);
  };

  DevToolsIOContext();
  ~DevToolsIOContext();

  scoped_refptr<Stream> GetByHandle(const std::string& handle);
  bool Close(const std::string& handle);
  void DiscardAllStreams();

  base::WeakPtr<DevToolsIOContext> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  static bool IsTextMimeType(const std::string& mime_type);

 private:
  // Registration can only be done by Stream subclasses through Stream methods.
  void RegisterStream(scoped_refptr<Stream> stream, const std::string& handle);

  std::map<std::string, scoped_refptr<Stream>> streams_;

  base::WeakPtrFactory<DevToolsIOContext> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_IO_CONTEXT_H_
