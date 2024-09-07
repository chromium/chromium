// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/base/compound_buffer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::IOBuffer;
using net::IOBufferWithSize;

namespace remoting {

namespace {
const int kDataSize = 1024;

// Chunk sizes used to append and prepend data to the buffer.
const int kChunkSizes0[] = {kDataSize, -1};
const int kChunkSizes1[] = {1, 10, 20, -1};

// Chunk sizes used to test CopyFrom().
const int kCopySizes0[] = {10, 3, -1};
const int kCopySizes1[] = {20, -1};

const int kCropSizes[] = {1, -1};

}  // namespace

class CompoundBufferTest : public testing::Test {
 public:
  // Following 5 methods are used with IterateOverPieces().
  void Append(int pos, int size) {
    target_.Append(data_, data_->data() + pos, size);
  }

  void AppendCopyOf(int pos, int size) {
    target_.AppendCopyOf(data_->data() + pos, size);
  }

  void Prepend(int pos, int size) {
    target_.Prepend(data_, data_->data() + kDataSize - pos - size, size);
  }

  void PrependCopyOf(int pos, int size) {
    target_.PrependCopyOf(data_->data() + (kDataSize - pos - size), size);
  }

  void TestCopyFrom(int pos, int size) {
    CompoundBuffer copy;
    copy.CopyFrom(target_, pos, pos + size);
    EXPECT_TRUE(CompareData(copy, data_->data() + pos, size));
  }

  void TestCropFront(int pos, int size) {
    CompoundBuffer cropped;
    cropped.CopyFrom(target_, 0, target_.total_bytes());
    cropped.CropFront(pos);
    EXPECT_TRUE(
        CompareData(cropped, data_->data() + pos, target_.total_bytes() - pos));
  }

  void TestCropBack(int pos, int size) {
    CompoundBuffer cropped;
    cropped.CopyFrom(target_, 0, target_.total_bytes());
    cropped.CropBack(pos);
    EXPECT_TRUE(
        CompareData(cropped, data_->data(), target_.total_bytes() - pos));
  }

 protected:
  void SetUp() override {
    data_ = base::MakeRefCounted<IOBufferWithSize>(kDataSize);
    for (int i = 0; i < kDataSize; ++i) {
      data_->data()[i] = i;
    }
  }

  // Iterate over chunks of data with sizes specified in |sizes| in the
  // interval [0..kDataSize]. |function| is called for each chunk.
  void IterateOverPieces(
      const int sizes[],
      const base::RepeatingCallback<void(int, int)>& function) {
    DCHECK_GT(sizes[0], 0);

    int pos = 0;
    int index = 0;
    while (pos < kDataSize) {
      int size = std::min(sizes[index], kDataSize - pos);
      ++index;
      if (sizes[index] <= 0) {
        index = 0;
      }

      function.Run(pos, size);

      pos += size;
    }
  }

  bool CompareData(const CompoundBuffer& buffer, char* data, int size) {
    scoped_refptr<IOBuffer> buffer_data = buffer.ToIOBufferWithSize();
    return buffer.total_bytes() == size &&
           memcmp(buffer_data->data(), data, size) == 0;
  }

  static size_t ReadFromInput(CompoundBufferInputStream* input,
                              void* data,
                              size_t size) {
    uint8_t* out = reinterpret_cast<uint8_t*>(data);
    int out_size = size;

    const void* in;
    int in_size = 0;

    while (true) {
      if (!input->Next(&in, &in_size)) {
        return size - out_size;
      }
      EXPECT_GT(in_size, -1);

      if (out_size <= in_size) {
        memcpy(out, in, out_size);
        if (in_size > out_size) {
          input->BackUp(in_size - out_size);
        }
        return size;  // Copied all of it.
      }

      memcpy(out, in, in_size);
      out += in_size;
      out_size -= in_size;
    }
  }

  static void ReadString(CompoundBufferInputStream* input,
                         const std::string& str) {
    SCOPED_TRACE(str);
    auto buffer = base::HeapArray<char>::Uninit(str.size() + 1);
    buffer[str.size()] = '\0';
    EXPECT_EQ(ReadFromInput(input, buffer.data(), str.size()), str.size());
    EXPECT_STREQ(str.data(), buffer.data());
  }

  // Construct and prepare data in the |buffer|.
  static void PrepareData(std::unique_ptr<CompoundBuffer>* buffer) {
    static const std::string kTestData =
        "Hello world!"
        "This is testing"
        "MultipleArrayInputStream"
        "for Chromoting";

    // Determine how many segments to split kTestData. We split the data in
    // 1 character, 2 characters, 1 character, 2 characters ...
    int segments = (kTestData.length() / 3) * 2;
    int remaining_chars = kTestData.length() % 3;
    if (remaining_chars) {
      if (remaining_chars == 1) {
        ++segments;
      } else {
        segments += 2;
      }
    }

    CompoundBuffer* result = new CompoundBuffer();
    const char* data = kTestData.data();
    for (int i = 0; i < segments; ++i) {
      size_t size = i % 2 == 0 ? 1 : 2;
      result->Append(base::MakeRefCounted<net::WrappedIOBuffer>(
                         base::make_span(data, size)),
                     size);
      data += size;
    }
    result->Lock();
    buffer->reset(result);
  }

  CompoundBuffer target_;
  scoped_refptr<IOBuffer> data_;
};

TEST_F(CompoundBufferTest, Append) {
  target_.Clear();
  IterateOverPieces(
      kChunkSizes0,
      base::BindRepeating(&CompoundBufferTest::Append, base::Unretained(this)));
  EXPECT_TRUE(CompareData(target_, data_->data(), kDataSize));

  target_.Clear();
  IterateOverPieces(
      kChunkSizes1,
      base::BindRepeating(&CompoundBufferTest::Append, base::Unretained(this)));
  EXPECT_TRUE(CompareData(target_, data_->data(), kDataSize));
}

TEST_F(CompoundBufferTest, AppendCopyOf) {
  target_.Clear();
  IterateOverPieces(kChunkSizes0,
                    base::BindRepeating(&CompoundBufferTest::AppendCopyOf,
                                        base::Unretained(this)));
  EXPECT_TRUE(CompareData(target_, data_->data(), kDataSize));

  target_.Clear();
  IterateOverPieces(kChunkSizes1,
                    base::BindRepeating(&CompoundBufferTest::AppendCopyOf,
                                        base::Unretained(this)));
  EXPECT_TRUE(CompareData(target_, data_->data(), kDataSize));
}

TEST_F(CompoundBufferTest, Prepend) {
  target_.Clear();
  IterateOverPieces(kChunkSizes0,
                    base::BindRepeating(&CompoundBufferTest::Prepend,
                                        base::Unretained(this)));
  EXPECT_TRUE(CompareData(target_, data_->data(), kDataSize));

  target_.Clear();
  IterateOverPieces(kChunkSizes1,
                    base::BindRepeating(&CompoundBufferTest::Prepend,
                                        base::Unretained(this)));
  EXPECT_TRUE(CompareData(target_, data_->data(), kDataSize));
}

TEST_F(CompoundBufferTest, PrependCopyOf) {
  target_.Clear();
  IterateOverPieces(kChunkSizes0,
                    base::BindRepeating(&CompoundBufferTest::PrependCopyOf,
                                        base::Unretained(this)));
  EXPECT_TRUE(CompareData(target_, data_->data(), kDataSize));

  target_.Clear();
  IterateOverPieces(kChunkSizes1,
                    base::BindRepeating(&CompoundBufferTest::PrependCopyOf,
                                        base::Unretained(this)));
  EXPECT_TRUE(CompareData(target_, data_->data(), kDataSize));
}

TEST_F(CompoundBufferTest, CropFront) {
  target_.Clear();
  IterateOverPieces(
      kChunkSizes1,
      base::BindRepeating(&CompoundBufferTest::Append, base::Unretained(this)));
  IterateOverPieces(kCropSizes,
                    base::BindRepeating(&CompoundBufferTest::TestCropFront,
                                        base::Unretained(this)));
}

TEST_F(CompoundBufferTest, CropBack) {
  target_.Clear();
  IterateOverPieces(
      kChunkSizes1,
      base::BindRepeating(&CompoundBufferTest::Append, base::Unretained(this)));
  IterateOverPieces(kCropSizes,
                    base::BindRepeating(&CompoundBufferTest::TestCropBack,
                                        base::Unretained(this)));
}

TEST_F(CompoundBufferTest, CopyFrom) {
  target_.Clear();
  IterateOverPieces(
      kChunkSizes1,
      base::BindRepeating(&CompoundBufferTest::Append, base::Unretained(this)));
  {
    SCOPED_TRACE("CopyFrom.kCopySizes0");
    IterateOverPieces(kCopySizes0,
                      base::BindRepeating(&CompoundBufferTest::TestCopyFrom,
                                          base::Unretained(this)));
  }
  {
    SCOPED_TRACE("CopyFrom.kCopySizes1");
    IterateOverPieces(kCopySizes1,
                      base::BindRepeating(&CompoundBufferTest::TestCopyFrom,
                                          base::Unretained(this)));
  }
}

TEST_F(CompoundBufferTest, InputStream) {
  std::unique_ptr<CompoundBuffer> buffer;
  PrepareData(&buffer);
  CompoundBufferInputStream stream(buffer.get());

  ReadString(&stream, "Hello world!");
  ReadString(&stream, "This ");
  ReadString(&stream, "is test");
  EXPECT_TRUE(stream.Skip(3));
  ReadString(&stream, "MultipleArrayInput");
  EXPECT_TRUE(stream.Skip(6));
  ReadString(&stream, "f");
  ReadString(&stream, "o");
  ReadString(&stream, "r");
  ReadString(&stream, " ");
  ReadString(&stream, "Chromoting");
}

}  // namespace remoting
