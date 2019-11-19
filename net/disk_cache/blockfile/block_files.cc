// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/block_files.h"

#include <atomic>
#include <limits>

#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "net/disk_cache/blockfile/file_lock.h"
#include "net/disk_cache/blockfile/stress_support.h"
#include "net/disk_cache/blockfile/trace.h"
#include "net/disk_cache/cache_util.h"

using base::TimeTicks;

namespace {

const char kBlockName[] = "data_";

// This array is used to perform a fast lookup of the nibble bit pattern to the
// type of entry that can be stored there (number of consecutive blocks).
const char s_types[16] = {4, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};

// Returns the type of block (number of consecutive blocks that can be stored)
// for a given nibble of the bitmap.
inline int GetMapBlockType(uint32_t value) {
  value &= 0xf;
  return s_types[value];
}

}  // namespace

namespace disk_cache {

BlockHeader::BlockHeader() : header_(nullptr) {}

BlockHeader::BlockHeader(BlockFileHeader* header) : header_(header) {
}

BlockHeader::BlockHeader(MappedFile* file)
    : header_(reinterpret_cast<BlockFileHeader*>(file->buffer())) {
}

BlockHeader::BlockHeader(const BlockHeader& other) = default;

BlockHeader::~BlockHeader() = default;

bool BlockHeader::CreateMapBlock(int size, int* index) {
  DCHECK(size > 0 && size <= kMaxNumBlocks);
  int target = 0;
  for (int i = size; i <= kMaxNumBlocks; i++) {
    if (header_->empty[i - 1]) {
      target = i;
      break;
    }
  }

  if (!target) {
    STRESS_NOTREACHED();
    return false;
  }

  TimeTicks start = TimeTicks::Now();
  // We are going to process the map on 32-block chunks (32 bits), and on every
  // chunk, iterate through the 8 nibbles where the new block can be located.
  int current = header_->hints[target - 1];
  for (int i = 0; i < header_->max_entries / 32; i++, current++) {
    if (current == header_->max_entries / 32)
      current = 0;
    uint32_t map_block = header_->allocation_map[current];

    for (int j = 0; j < 8; j++, map_block >>= 4) {
      if (GetMapBlockType(map_block) != target)
        continue;

      disk_cache::FileLock lock(header_);
      int index_offset = j * 4 + 4 - target;
      *index = current * 32 + index_offset;
      STRESS_DCHECK(*index / 4 == (*index + size - 1) / 4);
      uint32_t to_add = ((1 << size) - 1) << index_offset;
      header_->num_entries++;

      // Note that there is no race in the normal sense here, but if we enforce
      // the order of memory accesses between num_entries and allocation_map, we
      // can assert that even if we crash here, num_entries will never be less
      // than the actual number of used blocks.
      std::atomic_thread_fence(std::memory_order_seq_cst);
      header_->allocation_map[current] |= to_add;

      header_->hints[target - 1] = current;
      header_->empty[target - 1]--;
      STRESS_DCHECK(header_->empty[target - 1] >= 0);
      if (target != size) {
        header_->empty[target - size - 1]++;
      }
      LOCAL_HISTOGRAM_TIMES("DiskCache.CreateBlock", TimeTicks::Now() - start);
      return true;
    }
  }

  // It is possible to have an undetected corruption (for example when the OS
  // crashes), fix it here.
  LOG(ERROR) << "Failing CreateMapBlock";
  FixAllocationCounters();
  return false;
}

void BlockHeader::DeleteMapBlock(int index, int size) {
  if (size < 0 || size > kMaxNumBlocks) {
    NOTREACHED();
    return;
  }
  TimeTicks start = TimeTicks::Now();
  int byte_index = index / 8;
  uint8_t* byte_map = reinterpret_cast<uint8_t*>(header_->allocation_map);
  uint8_t map_block = byte_map[byte_index];

  if (index % 8 >= 4)
    map_block >>= 4;

  // See what type of block will be available after we delete this one.
  int bits_at_end = 4 - size - index % 4;
  uint8_t end_mask = (0xf << (4 - bits_at_end)) & 0xf;
  bool update_counters = (map_block & end_mask) == 0;
  uint8_t new_value = map_block & ~(((1 << size) - 1) << (index % 4));
  int new_type = GetMapBlockType(new_value);

  disk_cache::FileLock lock(header_);
  STRESS_DCHECK((((1 << size) - 1) << (index % 8)) < 0x100);
  uint8_t to_clear = ((1 << size) - 1) << (index % 8);
  STRESS_DCHECK((byte_map[byte_index] & to_clear) == to_clear);
  byte_map[byte_index] &= ~to_clear;

  if (update_counters) {
    if (bits_at_end)
      header_->empty[bits_at_end - 1]--;
    header_->empty[new_type - 1]++;
    STRESS_DCHECK(header_->empty[bits_at_end - 1] >= 0);
  }
  std::atomic_thread_fence(std::memory_order_seq_cst);
  header_->num_entries--;
  STRESS_DCHECK(header_->num_entries >= 0);
  LOCAL_HISTOGRAM_TIMES("DiskCache.DeleteBlock", TimeTicks::Now() - start);
}

// Note that this is a simplified version of DeleteMapBlock().
bool BlockHeader::UsedMapBlock(int index, int size) {
  if (size < 0 || size > kMaxNumBlocks)
    return false;

  int byte_index = index / 8;
  uint8_t* byte_map = reinterpret_cast<uint8_t*>(header_->allocation_map);
  uint8_t map_block = byte_map[byte_index];

  if (index % 8 >= 4)
    map_block >>= 4;

  STRESS_DCHECK((((1 << size) - 1) << (index % 8)) < 0x100);
  uint8_t to_clear = ((1 << size) - 1) << (index % 8);
  return ((byte_map[byte_index] & to_clear) == to_clear);
}

void BlockHeader::FixAllocationCounters() {
  for (int i = 0; i < kMaxNumBlocks; i++) {
    header_->hints[i] = 0;
    header_->empty[i] = 0;
  }

  for (int i = 0; i < header_->max_entries / 32; i++) {
    uint32_t map_block = header_->allocation_map[i];

    for (int j = 0; j < 8; j++, map_block >>= 4) {
      int type = GetMapBlockType(map_block);
      if (type)
        header_->empty[type -1]++;
    }
  }
}

bool BlockHeader::NeedToGrowBlockFile(int block_count) const {
  bool have_space = false;
  int empty_blocks = 0;
  for (int i = 0; i < kMaxNumBlocks; i++) {
    empty_blocks += header_->empty[i] * (i + 1);
    if (i >= block_count - 1 && header_->empty[i])
      have_space = true;
  }

  if (header_->next_file && (empty_blocks < kMaxBlocks / 10)) {
    // This file is almost full but we already created another one, don't use
    // this file yet so that it is easier to find empty blocks when we start
    // using this file again.
    return true;
  }
  return !have_space;
}

bool BlockHeader::CanAllocate(int block_count) const {
  DCHECK_GT(block_count, 0);
  for (int i = block_count - 1; i < kMaxNumBlocks; i++) {
    if (header_->empty[i])
      return true;
  }

  return false;
}

int BlockHeader::EmptyBlocks() const {
  int empty_blocks = 0;
  for (int i = 0; i < kMaxNumBlocks; i++) {
    empty_blocks += header_->empty[i] * (i + 1);
    if (header_->empty[i] < 0)
      return 0;
  }
  return empty_blocks;
}

int BlockHeader::MinimumAllocations() const {
  return header_->empty[kMaxNumBlocks - 1];
}

int BlockHeader::Capacity() const {
  return header_->max_entries;
}

bool BlockHeader::ValidateCounters() const {
  if (header_->max_entries < 0 || header_->max_entries > kMaxBlocks ||
      header_->num_entries < 0)
    return false;

  int empty_blocks = EmptyBlocks();
  if (empty_blocks + header_->num_entries > header_->max_entries)
    return false;

  return true;
}

int BlockHeader::FileId() const {
  return header_->this_file;
}

int BlockHeader::NextFileId() const {
  return header_->next_file;
}

int BlockHeader::Size() const {
  return static_cast<int>(sizeof(*header_));
}

BlockFileHeader* BlockHeader::Header() {
  return header_;
}

// ------------------------------------------------------------------------

BlockFiles::BlockFiles(const base::FilePath& path)
    : init_(false), zero_buffer_(nullptr), path_(path) {}

BlockFiles::~BlockFiles() {
  if (zero_buffer_)
    delete[] zero_buffer_;
  CloseFiles();
}

bool BlockFiles::Init(bool create_files) {
  DCHECK(!init_);
  if (init_)
    return false;

  thread_checker_.reset(new base::ThreadChecker);

  block_files_.resize(kFirstAdditionalBlockFile);
  for (int16_t i = 0; i < kFirstAdditionalBlockFile; i++) {
    if (create_files)
      if (!CreateBlockFile(i, static_cast<FileType>(i + 1), true))
        return false;

    if (!OpenBlockFile(i))
      return false;

    // Walk this chain of files removing empty ones.
    if (!RemoveEmptyFile(static_cast<FileType>(i + 1)))
      return false;
  }

  init_ = true;
  return true;
}

MappedFile* BlockFiles::GetFile(Addr address) {
  DCHECK(thread_checker_->CalledOnValidThread());
  DCHECK_GE(block_files_.size(),
            static_cast<size_t>(kFirstAdditionalBlockFile));
  DCHECK(address.is_block_file() || !address.is_initialized());
  if (!address.is_initialized())
    return nullptr;

  int file_index = address.FileNumber();
  if (static_cast<unsigned int>(file_index) >= block_files_.size() ||
      !block_files_[file_index]) {
    // We need to open the file
    if (!OpenBlockFile(file_index))
      return nullptr;
  }
  DCHECK_GE(block_files_.size(), static_cast<unsigned int>(file_index));
  return block_files_[file_index].get();
}

bool BlockFiles::CreateBlock(FileType block_type, int block_count,
                             Addr* block_address) {
  DCHECK(thread_checker_->CalledOnValidThread());
  DCHECK_NE(block_type, EXTERNAL);
  DCHECK_NE(block_type, BLOCK_FILES);
  DCHECK_NE(block_type, BLOCK_ENTRIES);
  DCHECK_NE(block_type, BLOCK_EVICTED);
  if (block_count < 1 || block_count > kMaxNumBlocks)
    return false;

  if (!init_)
    return false;

  MappedFile* file = FileForNewBlock(block_type, block_count);
  if (!file)
    return false;

  ScopedFlush flush(file);
  BlockHeader file_header(file);

  int index;
  if (!file_header.CreateMapBlock(block_count, &index))
    return false;

  Addr address(block_type, block_count, file_header.FileId(), index);
  block_address->set_value(address.value());
  Trace("CreateBlock 0x%x", address.value());
  return true;
}

void BlockFiles::DeleteBlock(Addr address, bool deep) {
  DCHECK(thread_checker_->CalledOnValidThread());
  if (!address.is_initialized() || address.is_separate_file())
    return;

  if (!zero_buffer_) {
    zero_buffer_ = new char[Addr::BlockSizeForFileType(BLOCK_4K) * 4];
    memset(zero_buffer_, 0, Addr::BlockSizeForFileType(BLOCK_4K) * 4);
  }
  MappedFile* file = GetFile(address);
  if (!file)
    return;

  Trace("DeleteBlock 0x%x", address.value());

  size_t size = address.BlockSize() * address.num_blocks();
  size_t offset = address.start_block() * address.BlockSize() +
                  kBlockHeaderSize;
  if (deep)
    file->Write(zero_buffer_, size, offset);

  BlockHeader file_header(file);
  file_header.DeleteMapBlock(address.start_block(), address.num_blocks());
  file->Flush();

  if (!file_header.Header()->num_entries) {
    // This file is now empty. Let's try to delete it.
    FileType type = Addr::RequiredFileType(file_header.Header()->entry_size);
    if (Addr::BlockSizeForFileType(RANKINGS) ==
        file_header.Header()->entry_size) {
      type = RANKINGS;
    }
    RemoveEmptyFile(type);  // Ignore failures.
  }
}

void BlockFiles::CloseFiles() {
  if (init_) {
    DCHECK(thread_checker_->CalledOnValidThread());
  }
  init_ = false;
  block_files_.clear();
}

void BlockFiles::ReportStats() {
  DCHECK(thread_checker_->CalledOnValidThread());
  int used_blocks[kFirstAdditionalBlockFile];
  int load[kFirstAdditionalBlockFile];
  for (int i = 0; i < kFirstAdditionalBlockFile; i++) {
    GetFileStats(i, &used_blocks[i], &load[i]);
  }
  UMA_HISTOGRAM_COUNTS_1M("DiskCache.Blocks_0", used_blocks[0]);
  UMA_HISTOGRAM_COUNTS_1M("DiskCache.Blocks_1", used_blocks[1]);
  UMA_HISTOGRAM_COUNTS_1M("DiskCache.Blocks_2", used_blocks[2]);
  UMA_HISTOGRAM_COUNTS_1M("DiskCache.Blocks_3", used_blocks[3]);

  UMA_HISTOGRAM_ENUMERATION("DiskCache.BlockLoad_0", load[0], 101);
  UMA_HISTOGRAM_ENUMERATION("DiskCache.BlockLoad_1", load[1], 101);
  UMA_HISTOGRAM_ENUMERATION("DiskCache.BlockLoad_2", load[2], 101);
  UMA_HISTOGRAM_ENUMERATION("DiskCache.BlockLoad_3", load[3], 101);
}

bool BlockFiles::IsValid(Addr address) {
#ifdef NDEBUG
  return true;
#else
  if (!address.is_initialized() || address.is_separate_file())
    return false;

  MappedFile* file = GetFile(address);
  if (!file)
    return false;

  BlockHeader header(file);
  bool rv = header.UsedMapBlock(address.start_block(), address.num_blocks());
  DCHECK(rv);

  static bool read_contents = false;
  if (read_contents) {
    std::unique_ptr<char[]> buffer;
    buffer.reset(new char[Addr::BlockSizeForFileType(BLOCK_4K) * 4]);
    size_t size = address.BlockSize() * address.num_blocks();
    size_t offset = address.start_block() * address.BlockSize() +
                    kBlockHeaderSize;
    bool ok = file->Read(buffer.get(), size, offset);
    DCHECK(ok);
  }

  return rv;
#endif
}

bool BlockFiles::CreateBlockFile(int index, FileType file_type, bool force) {
  base::FilePath name = Name(index);
  int flags = force ? base::File::FLAG_CREATE_ALWAYS : base::File::FLAG_CREATE;
  flags |= base::File::FLAG_WRITE | base::File::FLAG_EXCLUSIVE_WRITE;

  scoped_refptr<File> file(new File(base::File(name, flags)));
  if (!file->IsValid())
    return false;

  BlockFileHeader header;
  memset(&header, 0, sizeof(header));
  header.magic = kBlockMagic;
  header.version = kBlockVersion2;
  header.entry_size = Addr::BlockSizeForFileType(file_type);
  header.this_file = static_cast<int16_t>(index);
  DCHECK(index <= std::numeric_limits<int16_t>::max() && index >= 0);

  return file->Write(&header, sizeof(header), 0);
}

bool BlockFiles::OpenBlockFile(int index) {
  if (block_files_.size() - 1 < static_cast<unsigned int>(index)) {
    DCHECK(index > 0);
    int to_add = index - static_cast<int>(block_files_.size()) + 1;
    block_files_.resize(block_files_.size() + to_add);
  }

  base::FilePath name = Name(index);
  scoped_refptr<MappedFile> file(new MappedFile());

  if (!file->Init(name, kBlockHeaderSize)) {
    LOG(ERROR) << "Failed to open " << name.value();
    return false;
  }

  size_t file_len = file->GetLength();
  if (file_len < static_cast<size_t>(kBlockHeaderSize)) {
    LOG(ERROR) << "File too small " << name.value();
    return false;
  }

  BlockHeader file_header(file.get());
  BlockFileHeader* header = file_header.Header();
  if (kBlockMagic != header->magic || kBlockVersion2 != header->version) {
    LOG(ERROR) << "Invalid file version or magic " << name.value();
    return false;
  }

  if (header->updating || !file_header.ValidateCounters()) {
    // Last instance was not properly shutdown, or counters are out of sync.
    if (!FixBlockFileHeader(file.get())) {
      LOG(ERROR) << "Unable to fix block file " << name.value();
      return false;
    }
  }

  if (static_cast<int>(file_len) <
      header->max_entries * header->entry_size + kBlockHeaderSize) {
    LOG(ERROR) << "File too small " << name.value();
    return false;
  }

  if (index == 0) {
    // Load the links file into memory.
    if (!file->Preload())
      return false;
  }

  ScopedFlush flush(file.get());
  DCHECK(!block_files_[index]);
  block_files_[index] = std::move(file);
  return true;
}

bool BlockFiles::GrowBlockFile(MappedFile* file, BlockFileHeader* header) {
  if (kMaxBlocks == header->max_entries)
    return false;

  ScopedFlush flush(file);
  DCHECK(!header->empty[3]);
  int new_size = header->max_entries + 1024;
  if (new_size > kMaxBlocks)
    new_size = kMaxBlocks;

  int new_size_bytes = new_size * header->entry_size + sizeof(*header);

  if (!file->SetLength(new_size_bytes)) {
    // Most likely we are trying to truncate the file, so the header is wrong.
    if (header->updating < 10 && !FixBlockFileHeader(file)) {
      // If we can't fix the file increase the lock guard so we'll pick it on
      // the next start and replace it.
      header->updating = 100;
      return false;
    }
    return (header->max_entries >= new_size);
  }

  FileLock lock(header);
  header->empty[3] = (new_size - header->max_entries) / 4;  // 4 blocks entries
  header->max_entries = new_size;

  return true;
}

MappedFile* BlockFiles::FileForNewBlock(FileType block_type, int block_count) {
  static_assert(RANKINGS == 1, "invalid file type");
  MappedFile* file = block_files_[block_type - 1].get();
  BlockHeader file_header(file);

  TimeTicks start = TimeTicks::Now();
  while (file_header.NeedToGrowBlockFile(block_count)) {
    if (kMaxBlocks == file_header.Header()->max_entries) {
      file = NextFile(file);
      if (!file)
        return nullptr;
      file_header = BlockHeader(file);
      continue;
    }

    if (!GrowBlockFile(file, file_header.Header()))
      return nullptr;
    break;
  }
  LOCAL_HISTOGRAM_TIMES("DiskCache.GetFileForNewBlock",
                        TimeTicks::Now() - start);
  return file;
}

MappedFile* BlockFiles::NextFile(MappedFile* file) {
  ScopedFlush flush(file);
  BlockFileHeader* header = reinterpret_cast<BlockFileHeader*>(file->buffer());
  int16_t new_file = header->next_file;
  if (!new_file) {
    // RANKINGS is not reported as a type for small entries, but we may be
    // extending the rankings block file.
    FileType type = Addr::RequiredFileType(header->entry_size);
    if (header->entry_size == Addr::BlockSizeForFileType(RANKINGS))
      type = RANKINGS;

    new_file = CreateNextBlockFile(type);
    if (!new_file)
      return nullptr;

    FileLock lock(header);
    header->next_file = new_file;
  }

  // Only the block_file argument is relevant for what we want.
  Addr address(BLOCK_256, 1, new_file, 0);
  return GetFile(address);
}

int16_t BlockFiles::CreateNextBlockFile(FileType block_type) {
  for (int16_t i = kFirstAdditionalBlockFile; i <= kMaxBlockFile; i++) {
    if (CreateBlockFile(i, block_type, false))
      return i;
  }
  return 0;
}

// We walk the list of files for this particular block type, deleting the ones
// that are empty.
bool BlockFiles::RemoveEmptyFile(FileType block_type) {
  MappedFile* file = block_files_[block_type - 1].get();
  BlockFileHeader* header = reinterpret_cast<BlockFileHeader*>(file->buffer());

  while (header->next_file) {
    // Only the block_file argument is relevant for what we want.
    Addr address(BLOCK_256, 1, header->next_file, 0);
    MappedFile* next_file = GetFile(address);
    if (!next_file)
      return false;

    BlockFileHeader* next_header =
        reinterpret_cast<BlockFileHeader*>(next_file->buffer());
    if (!next_header->num_entries) {
      DCHECK_EQ(next_header->entry_size, header->entry_size);
      // Delete next_file and remove it from the chain.
      int file_index = header->next_file;
      header->next_file = next_header->next_file;
      DCHECK(block_files_.size() >= static_cast<unsigned int>(file_index));
      file->Flush();

      // We get a new handle to the file and release the old one so that the
      // file gets unmmaped... so we can delete it.
      base::FilePath name = Name(file_index);
      scoped_refptr<File> this_file(new File(false));
      this_file->Init(name);
      block_files_[file_index] = nullptr;

      int failure = DeleteCacheFile(name) ? 0 : 1;
      UMA_HISTOGRAM_COUNTS_1M("DiskCache.DeleteFailed2", failure);
      if (failure)
        LOG(ERROR) << "Failed to delete " << name.value() << " from the cache.";
      continue;
    }

    header = next_header;
    file = next_file;
  }
  return true;
}

// Note that we expect to be called outside of a FileLock... however, we cannot
// DCHECK on header->updating because we may be fixing a crash.
bool BlockFiles::FixBlockFileHeader(MappedFile* file) {
  ScopedFlush flush(file);
  BlockHeader file_header(file);
  int file_size = static_cast<int>(file->GetLength());
  if (file_size < file_header.Size())
    return false;  // file_size > 2GB is also an error.

  const int kMinHeaderBlockSize = 36;
  const int kMaxHeaderBlockSize = 4096;
  BlockFileHeader* header = file_header.Header();
  if (header->entry_size < kMinHeaderBlockSize ||
      header->entry_size > kMaxHeaderBlockSize || header->num_entries < 0)
    return false;

  // Make sure that we survive crashes.
  header->updating = 1;
  int expected = header->entry_size * header->max_entries + file_header.Size();
  if (file_size != expected) {
    int max_expected = header->entry_size * kMaxBlocks + file_header.Size();
    if (file_size < expected || header->empty[3] || file_size > max_expected) {
      NOTREACHED();
      LOG(ERROR) << "Unexpected file size";
      return false;
    }
    // We were in the middle of growing the file.
    int num_entries = (file_size - file_header.Size()) / header->entry_size;
    header->max_entries = num_entries;
  }

  file_header.FixAllocationCounters();
  int empty_blocks = file_header.EmptyBlocks();
  if (empty_blocks + header->num_entries > header->max_entries)
    header->num_entries = header->max_entries - empty_blocks;

  if (!file_header.ValidateCounters())
    return false;

  header->updating = 0;
  return true;
}

// We are interested in the total number of blocks used by this file type, and
// the max number of blocks that we can store (reported as the percentage of
// used blocks). In order to find out the number of used blocks, we have to
// substract the empty blocks from the total blocks for each file in the chain.
void BlockFiles::GetFileStats(int index, int* used_count, int* load) {
  int max_blocks = 0;
  *used_count = 0;
  *load = 0;
  for (;;) {
    if (!block_files_[index] && !OpenBlockFile(index))
      return;

    BlockFileHeader* header =
        reinterpret_cast<BlockFileHeader*>(block_files_[index]->buffer());

    max_blocks += header->max_entries;
    int used = header->max_entries;
    for (int i = 0; i < kMaxNumBlocks; i++) {
      used -= header->empty[i] * (i + 1);
      DCHECK_GE(used, 0);
    }
    *used_count += used;

    if (!header->next_file)
      break;
    index = header->next_file;
  }
  if (max_blocks)
    *load = *used_count * 100 / max_blocks;
}

base::FilePath BlockFiles::Name(int index) {
  // The file format allows for 256 files.
  DCHECK(index < 256 && index >= 0);
  std::string tmp = base::StringPrintf("%s%d", kBlockName, index);
  return path_.AppendASCII(tmp);
}

}  // namespace disk_cache
