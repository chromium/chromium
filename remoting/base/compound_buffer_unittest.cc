// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/compound_buffer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/numerics/safe_conversions.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::IOBuffer;
using net::IOBufferWithSize;

namespace remoting {

namespace {
const size_t kDataSize = 1024;

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
  // Following 7 methods are used with IterateOverPieces().
  void Append(size_t pos, size_t size) {
    target_.Append(data_, data_->span().subspan(pos, size));
  }

  void AppendCopyOf(size_t pos, size_t size) {
    target_.AppendCopyOf(data_->span().subspan(pos, size));
  }

  void Prepend(size_t pos, size_t size) {
    target_.Prepend(data_, data_->span().subspan(kDataSize - pos - size, size));
  }

  void PrependCopyOf(size_t pos, size_t size) {
    target_.PrependCopyOf(data_->span().subspan(kDataSize - pos - size, size));
  }

  void TestCopyFrom(size_t pos, size_t size) {
    CompoundBuffer copy;
    copy.CopyFrom(target_, pos, pos + size);
    UNSAFE_TODO(EXPECT_TRUE(CompareData(copy, data_->data() + pos, size)));
  }

  void TestCropFront(size_t pos, size_t size) {
    CompoundBuffer cropped;
    cropped.CopyFrom(target_, 0, target_.total_bytes());
    cropped.CropFront(pos);
    UNSAFE_TODO(EXPECT_TRUE(CompareData(cropped, data_->data() + pos,
                                        target_.total_bytes() - pos)));
  }

  void TestCropBack(size_t pos, size_t size) {
    CompoundBuffer cropped;
    cropped.CopyFrom(target_, 0, target_.total_bytes());
    cropped.CropBack(pos);
    EXPECT_TRUE(
        CompareData(cropped, data_->data(), target_.total_bytes() - pos));
  }

 protected:
  void SetUp() override {
    data_ = base::MakeRefCounted<IOBufferWithSize>(kDataSize);
    for (size_t i = 0; i < kDataSize; ++i) {
      data_->span()[i] = i;
    }
  }

  // Iterate over chunks of data with sizes specified in |sizes| in the
  // interval [0..kDataSize]. |function| is called for each chunk.
  void IterateOverPieces(
      base::span<const int> sizes,
      const base::RepeatingCallback<void(size_t, size_t)>& function) {
    DCHECK_GT(sizes[0], 0);

    size_t pos = 0;
    int index = 0;
    while (pos < kDataSize) {
      int size =
          std::min(sizes[index], base::checked_cast<int>(kDataSize - pos));
      ++index;
      if (sizes[index] <= 0) {
        index = 0;
      }

      function.Run(pos, size);

      pos += size;
    }
  }

  bool CompareData(const CompoundBuffer& buffer, char* data, size_t size) {
    scoped_refptr<IOBuffer> buffer_data = buffer.ToIOBufferWithSize();
    return buffer.total_bytes() == size &&
           UNSAFE_TODO(memcmp(buffer_data->data(), data, size)) == 0;
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
        UNSAFE_TODO(memcpy(out, in, out_size));
        if (in_size > out_size) {
          input->BackUp(in_size - out_size);
        }
        return size;  // Copied all of it.
      }

      UNSAFE_TODO(memcpy(out, in, in_size));
      UNSAFE_TODO(out += in_size);
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
                         UNSAFE_TODO(base::span(data, size))),
                     size);
      UNSAFE_TODO(data += size);
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
