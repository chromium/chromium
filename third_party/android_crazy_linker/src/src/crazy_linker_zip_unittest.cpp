// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_zip.h"

#include "crazy_linker_system_mock.h"

#include "crazy_linker_util.h"  // FOR CRAZY_OFFSET_FAILED?
#include "crazy_linker_zip_archive.h"
#include "crazy_linker_zip_test_data.h"

#include <gtest/gtest.h>

namespace crazy {

using namespace crazy::testing;

namespace {

// Convenience function to add a file to the mock filesystem from the
// data tables above.
void AddZipArchiveBytes(crazy::SystemMock* sys,
                        const char* file_name,
                        const unsigned char* data,
                        unsigned int data_size) {
  sys->AddRegularFile(file_name, reinterpret_cast<const char*>(data),
                      static_cast<size_t>(data_size));
}

// Find the EndOfCentralDirectory in a mutable byte array.
// Return nullptr if not found or if the array is too small.
ZipEndOfCentralDirectory* LocateEndOfCentralHeader(unsigned char* data,
                                                   size_t data_size) {
  if (data_size < sizeof(ZipEndOfCentralDirectory))
    return nullptr;

  size_t offset = data_size - sizeof(ZipEndOfCentralDirectory);
  for (;;) {
    auto* ptr = reinterpret_cast<ZipEndOfCentralDirectory*>(data + offset);
    if (ptr->signature == ptr->kExpectedSignature) {
      return ptr;
    }
    if (offset == 0)
      return nullptr;

    offset--;
  }
}

// Find the first central directory entry in a mutable byte array.
// On success, return pointer to the first entry, and sets |*directory_size|
// to the size of the directory in bytes.
ZipCentralDirHeader* LocateCentralDirectory(unsigned char* data,
                                            size_t data_size,
                                            size_t* directory_size) {
  ZipEndOfCentralDirectory* end_record =
      LocateEndOfCentralHeader(data, data_size);

  EXPECT_TRUE(end_record);
  *directory_size = end_record->central_directory_length;
  return reinterpret_cast<ZipCentralDirHeader*>(
      data + end_record->central_directory_start);
}

// Find the local file header of the first central directory entry.
ZipLocalFileHeader* LocateLocalFileHeader(unsigned char* data,
                                          size_t data_size) {
  size_t directory_size = 0;
  ZipCentralDirHeader* entry =
      LocateCentralDirectory(data, data_size, &directory_size);
  EXPECT_TRUE(entry);
  return reinterpret_cast<ZipLocalFileHeader*>(
      data + entry->relative_offset_of_local_header);
}

// Find the offset of the file 'hello_world.txt' in the zip archive
// stored at |data| with |data_size| bytes. Return CRAZY_OFFSET_FAILED if
// not found.
int32_t FindHelloWorldZipFileOffset(const unsigned char* data,
                                    size_t data_size) {
  SystemMock sys;
  AddZipArchiveBytes(&sys, "hello.zip", data, data_size);
  return FindStartOffsetOfFileInZipFile("hello.zip", "hello_world.txt");
}

}  // namespace

TEST(ZipParser, EmptyFile) {
  SystemMock sys;
  static const char kFilePath[] = "empty-file";
  sys.AddRegularFile(kFilePath, "", 0);

  EXPECT_EQ(CRAZY_OFFSET_FAILED,
            FindStartOffsetOfFileInZipFile(kFilePath, "hello"));
}

TEST(ZipParser, EmptyArchive) {
  SystemMock sys;
  static const char kFilePath[] = "empty.zip";
  AddZipArchiveBytes(&sys, kFilePath, empty_archive_zip, empty_archive_zip_len);

  EXPECT_EQ(CRAZY_OFFSET_FAILED,
            FindStartOffsetOfFileInZipFile(kFilePath, "hello"));
}

TEST(ZipParse, SimpleArchive) {
  SystemMock sys;
  static const char kFilePath[] = "hello.zip";
  AddZipArchiveBytes(&sys, kFilePath, hello_zip, hello_zip_len);

  EXPECT_EQ(CRAZY_OFFSET_FAILED,
            FindStartOffsetOfFileInZipFile(kFilePath, "hello"));

  int off = FindStartOffsetOfFileInZipFile(kFilePath, "hello_world.txt");
  EXPECT_GT(off, 0);
  EXPECT_LT(off, static_cast<int>(hello_zip_len));

  static const char kExpected[] = "Hello World Hello World\n";
  const size_t kExpectedSize = sizeof(kExpected) - 1;

  ASSERT_LT(kExpectedSize, static_cast<size_t>(hello_zip_len));
  EXPECT_LT(off, static_cast<int>(hello_zip_len - kExpectedSize));
}

TEST(ZipParse, SimpleArchiveWithCompressedFile) {
  EXPECT_EQ(CRAZY_OFFSET_FAILED,
            FindHelloWorldZipFileOffset(hello_compressed_zip,
                                        hello_compressed_zip_len));
}

TEST(ZipParse, CorruptedEndOfCentralHeader) {
  // Copy hello.zip into buffer.
  unsigned char hello_data[hello_zip_len];
  memcpy(hello_data, hello_zip, hello_zip_len);

  // Locate the end-of-central-directory record, then corrupt its signature.
  ZipEndOfCentralDirectory* end_record =
      LocateEndOfCentralHeader(hello_data, hello_zip_len);
  ASSERT_TRUE(end_record);
  end_record->signature = 0xdeadbeef;

  // Now ensure it doesn't parse correctly anymore.
  EXPECT_EQ(CRAZY_OFFSET_FAILED,
            FindHelloWorldZipFileOffset(hello_data, hello_zip_len));
}

TEST(ZipParse, CorruptedCentralDirectoryOffset) {
  // Copy hello.zip into buffer.
  unsigned char hello_data[hello_zip_len];
  memcpy(hello_data, hello_zip, hello_zip_len);

  // Locate the end-of-central-directory record, then corrupt the
  // central directory offset.
  ZipEndOfCentralDirectory* end_record =
      LocateEndOfCentralHeader(hello_data, hello_zip_len);
  ASSERT_TRUE(end_record);
  end_record->central_directory_start = hello_zip_len;

  // Now ensure it doesn't parse correctly anymore.
  EXPECT_EQ(CRAZY_OFFSET_FAILED,
            FindHelloWorldZipFileOffset(hello_data, hello_zip_len));
}

TEST(ZipParse, CorruptedCentralDirectoryLength) {
  // Copy hello.zip into buffer.
  unsigned char hello_data[hello_zip_len];
  memcpy(hello_data, hello_zip, hello_zip_len);

  // Locate the end-of-central-directory record, then corrupt the
  // central directory length by making it far too large.
  ZipEndOfCentralDirectory* end_record =
      LocateEndOfCentralHeader(hello_data, hello_zip_len);
  ASSERT_TRUE(end_record);
  end_record->central_directory_length = hello_zip_len;

  // Now ensure it doesn't parse correctly anymore.
  EXPECT_EQ(CRAZY_OFFSET_FAILED,
            FindHelloWorldZipFileOffset(hello_data, hello_zip_len));
}

TEST(ZipParse, CorruptedCentralDirectoryLength2) {
  // Copy hello.zip into buffer.
  unsigned char hello_data[hello_zip_len];
  memcpy(hello_data, hello_zip, hello_zip_len);

  // Locate the end-of-central-directory record, then corrupt the
  // central directory length, this time by making it too small.
  ZipEndOfCentralDirectory* end_record =
      LocateEndOfCentralHeader(hello_data, hello_zip_len);
  ASSERT_TRUE(end_record);
  end_record->central_directory_length = sizeof(ZipCentralDirHeader) - 2;

  // Now ensure it doesn't parse correctly anymore.
  EXPECT_EQ(CRAZY_OFFSET_FAILED,
            FindHelloWorldZipFileOffset(hello_data, hello_zip_len));
}

TEST(ZipParse, CorruptedCentralDirectoryEntry) {
  // Copy hello.zip into buffer.
  unsigned char hello_data[hello_zip_len];
  memcpy(hello_data, hello_zip, hello_zip_len);

  // Locate the central directory start, then corrupt the signature of the
  // first entry!.
  size_t directory_size = 0;
  ZipCentralDirHeader* central_directory =
      LocateCentralDirectory(hello_data, hello_zip_len, &directory_size);
  ASSERT_TRUE(central_directory);
  ASSERT_GT(directory_size, sizeof(ZipCentralDirHeader));
  central_directory->signature = 0xdeadbeef;

  // Now ensure it doesn't parse correctly anymore.
  EXPECT_EQ(CRAZY_OFFSET_FAILED,
            FindHelloWorldZipFileOffset(hello_data, hello_zip_len));
}

TEST(ZipParse, CorruptedLocalFileHeaderOffset) {
  // Copy hello.zip into buffer.
  unsigned char hello_data[hello_zip_len];
  memcpy(hello_data, hello_zip, hello_zip_len);

  // Locate the central directory start, then corrupt the signature of the
  // first entry!.
  size_t directory_size = 0;
  ZipCentralDirHeader* central_directory =
      LocateCentralDirectory(hello_data, hello_zip_len, &directory_size);
  ASSERT_TRUE(central_directory);
  ASSERT_GT(directory_size, sizeof(ZipCentralDirHeader));
  central_directory->relative_offset_of_local_header = hello_zip_len - 8;

  // Now ensure it doesn't parse correctly anymore.
  EXPECT_EQ(CRAZY_OFFSET_FAILED,
            FindHelloWorldZipFileOffset(hello_data, hello_zip_len));
}

TEST(ZipParse, CorruptedLocalFileHeaderSignature) {
  // Copy hello.zip into buffer.
  unsigned char hello_data[hello_zip_len];
  memcpy(hello_data, hello_zip, hello_zip_len);

  // Locate the local file header, then corrupt its signature.
  ZipLocalFileHeader* header = LocateLocalFileHeader(hello_data, hello_zip_len);
  header->signature = 0xdeadbeef;

  // Now ensure it doesn't parse correctly anymore.
  EXPECT_EQ(CRAZY_OFFSET_FAILED,
            FindHelloWorldZipFileOffset(hello_data, hello_zip_len));
}

TEST(ZipParse, CorruptedLocalFileHeaderSize) {
  // Copy hello.zip into buffer.
  unsigned char hello_data[hello_zip_len];
  memcpy(hello_data, hello_zip, hello_zip_len);

  // Locate the local file header, then corrupt its size to overflow
  // over the whole file.
  ZipLocalFileHeader* header = LocateLocalFileHeader(hello_data, hello_zip_len);
  header->extra_field_length = hello_zip_len;

  // Now ensure it doesn't parse correctly anymore.
  EXPECT_EQ(CRAZY_OFFSET_FAILED,
            FindHelloWorldZipFileOffset(hello_data, hello_zip_len));
}

}  // namespace crazy
