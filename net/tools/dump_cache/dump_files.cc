// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Performs basic inspection of the disk cache files with minimal disruption
// to the actual files (they still may change if an error is detected on the
// files).

#include "net/tools/dump_cache/dump_files.h"

#include <stdio.h>

#include <memory>
#include <set>
#include <string>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_executor.h"
#include "net/disk_cache/blockfile/block_files.h"
#include "net/disk_cache/blockfile/disk_format.h"
#include "net/disk_cache/blockfile/mapped_file.h"
#include "net/disk_cache/blockfile/stats.h"
#include "net/disk_cache/blockfile/storage_block-inl.h"
#include "net/disk_cache/blockfile/storage_block.h"
#include "net/url_request/view_cache_helper.h"

namespace {

const base::FilePath::CharType kIndexName[] = FILE_PATH_LITERAL("index");

// Reads the |header_size| bytes from the beginning of file |name|.
bool ReadHeader(const base::FilePath& name, char* header, int header_size) {
  base::File file(name, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    printf("Unable to open file %s\n", name.MaybeAsASCII().c_str());
    return false;
  }

  int read = file.Read(0, header, header_size);
  if (read != header_size) {
    printf("Unable to read file %s\n", name.MaybeAsASCII().c_str());
    return false;
  }
  return true;
}

int GetMajorVersionFromFile(const base::FilePath& name) {
  disk_cache::IndexHeader header;
  if (!ReadHeader(name, reinterpret_cast<char*>(&header), sizeof(header)))
    return 0;

  return header.version >> 16;
}

// Dumps the contents of the Stats record.
void DumpStats(const base::FilePath& path, disk_cache::CacheAddr addr) {
  // We need a task executor, although we really don't run any task.
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);

  disk_cache::BlockFiles block_files(path);
  if (!block_files.Init(false)) {
    printf("Unable to init block files\n");
    return;
  }

  disk_cache::Addr address(addr);
  disk_cache::MappedFile* file = block_files.GetFile(address);
  if (!file)
    return;

  size_t length = (2 + disk_cache::Stats::kDataSizesLength) * sizeof(int32_t) +
                  disk_cache::Stats::MAX_COUNTER * sizeof(int64_t);

  size_t offset = address.start_block() * address.BlockSize() +
                  disk_cache::kBlockHeaderSize;

  std::unique_ptr<int32_t[]> buffer(new int32_t[length]);
  if (!file->Read(buffer.get(), length, offset))
    return;

  printf("Stats:\nSignatrure: 0x%x\n", buffer[0]);
  printf("Total size: %d\n", buffer[1]);
  for (int i = 0; i < disk_cache::Stats::kDataSizesLength; i++)
    printf("Size(%d): %d\n", i, buffer[i + 2]);

  int64_t* counters = reinterpret_cast<int64_t*>(
      buffer.get() + 2 + disk_cache::Stats::kDataSizesLength);
  for (int i = 0; i < disk_cache::Stats::MAX_COUNTER; i++)
    printf("Count(%d): %" PRId64 "\n", i, *counters++);
  printf("-------------------------\n\n");
}

// Dumps the contents of the Index-file header.
void DumpIndexHeader(const base::FilePath& name,
                     disk_cache::CacheAddr* stats_addr) {
  disk_cache::IndexHeader header;
  if (!ReadHeader(name, reinterpret_cast<char*>(&header), sizeof(header)))
    return;

  printf("Index file:\n");
  printf("magic: %x\n", header.magic);
  printf("version: %d.%d\n", header.version >> 16, header.version & 0xffff);
  printf("entries: %d\n", header.num_entries);
  printf("total bytes: %d\n", header.num_bytes);
  printf("last file number: %d\n", header.last_file);
  printf("current id: %d\n", header.this_id);
  printf("table length: %d\n", header.table_len);
  printf("last crash: %d\n", header.crash);
  printf("experiment: %d\n", header.experiment);
  printf("stats: %x\n", header.stats);
  for (int i = 0; i < 5; i++) {
    printf("head %d: 0x%x\n", i, header.lru.heads[i]);
    printf("tail %d: 0x%x\n", i, header.lru.tails[i]);
    printf("size %d: 0x%x\n", i, header.lru.sizes[i]);
  }
  printf("transaction: 0x%x\n", header.lru.transaction);
  printf("operation: %d\n", header.lru.operation);
  printf("operation list: %d\n", header.lru.operation_list);
  printf("-------------------------\n\n");

  if (stats_addr)
    *stats_addr = header.stats;
}

// Dumps the contents of a block-file header.
void DumpBlockHeader(const base::FilePath& name) {
  disk_cache::BlockFileHeader header;
  if (!ReadHeader(name, reinterpret_cast<char*>(&header), sizeof(header)))
    return;

  printf("Block file: %s\n", name.BaseName().MaybeAsASCII().c_str());
  printf("magic: %x\n", header.magic);
  printf("version: %d.%d\n", header.version >> 16, header.version & 0xffff);
  printf("file id: %d\n", header.this_file);
  printf("next file id: %d\n", header.next_file);
  printf("entry size: %d\n", header.entry_size);
  printf("current entries: %d\n", header.num_entries);
  printf("max entries: %d\n", header.max_entries);
  printf("updating: %d\n", header.updating);
  printf("empty sz 1: %d\n", header.empty[0]);
  printf("empty sz 2: %d\n", header.empty[1]);
  printf("empty sz 3: %d\n", header.empty[2]);
  printf("empty sz 4: %d\n", header.empty[3]);
  printf("user 0: 0x%x\n", header.user[0]);
  printf("user 1: 0x%x\n", header.user[1]);
  printf("user 2: 0x%x\n", header.user[2]);
  printf("user 3: 0x%x\n", header.user[3]);
  printf("-------------------------\n\n");
}

// Simple class that interacts with the set of cache files.
class CacheDumper {
 public:
  explicit CacheDumper(const base::FilePath& path)
      : path_(path),
        block_files_(path),
        index_(nullptr),
        current_hash_(0),
        next_addr_(0) {}

  bool Init();

  // Reads an entry from disk. Return false when all entries have been already
  // returned.
  bool GetEntry(disk_cache::EntryStore* entry, disk_cache::CacheAddr* addr);

  // Loads a specific block from the block files.
  bool LoadEntry(disk_cache::CacheAddr addr, disk_cache::EntryStore* entry);
  bool LoadRankings(disk_cache::CacheAddr addr,
                    disk_cache::RankingsNode* rankings);

  // Appends the data store at |addr| to |out|.
  bool HexDump(disk_cache::CacheAddr addr, std::string* out);

 private:
  base::FilePath path_;
  disk_cache::BlockFiles block_files_;
  scoped_refptr<disk_cache::MappedFile> index_file_;
  disk_cache::Index* index_;
  int current_hash_;
  disk_cache::CacheAddr next_addr_;
  std::set<disk_cache::CacheAddr> dumped_entries_;
  DISALLOW_COPY_AND_ASSIGN(CacheDumper);
};

bool CacheDumper::Init() {
  if (!block_files_.Init(false)) {
    printf("Unable to init block files\n");
    return false;
  }

  base::FilePath index_name(path_.Append(kIndexName));
  index_file_ = new disk_cache::MappedFile;
  index_ = reinterpret_cast<disk_cache::Index*>(
      index_file_->Init(index_name, 0));
  if (!index_) {
    printf("Unable to map index\n");
    return false;
  }

  return true;
}

bool CacheDumper::GetEntry(disk_cache::EntryStore* entry,
                           disk_cache::CacheAddr* addr) {
  if (dumped_entries_.find(next_addr_) != dumped_entries_.end()) {
    printf("Loop detected\n");
    next_addr_ = 0;
    current_hash_++;
  }

  if (next_addr_) {
    *addr = next_addr_;
    if (LoadEntry(next_addr_, entry))
      return true;

    printf("Unable to load entry at address 0x%x\n", next_addr_);
    next_addr_ = 0;
    current_hash_++;
  }

  for (int i = current_hash_; i < index_->header.table_len; i++) {
    // Yes, we'll crash if the table is shorter than expected, but only after
    // dumping every entry that we can find.
    if (index_->table[i]) {
      current_hash_ = i;
      *addr = index_->table[i];
      if (LoadEntry(index_->table[i], entry))
        return true;

      printf("Unable to load entry at address 0x%x\n", index_->table[i]);
    }
  }
  return false;
}

bool CacheDumper::LoadEntry(disk_cache::CacheAddr addr,
                            disk_cache::EntryStore* entry) {
  disk_cache::Addr address(addr);
  disk_cache::MappedFile* file = block_files_.GetFile(address);
  if (!file)
    return false;

  disk_cache::StorageBlock<disk_cache::EntryStore> entry_block(file, address);
  if (!entry_block.Load())
    return false;

  memcpy(entry, entry_block.Data(), sizeof(*entry));
  if (!entry_block.VerifyHash())
    printf("Self hash failed at 0x%x\n", addr);

  // Prepare for the next entry to load.
  next_addr_ = entry->next;
  if (next_addr_) {
    dumped_entries_.insert(addr);
  } else {
    current_hash_++;
    dumped_entries_.clear();
  }
  return true;
}

bool CacheDumper::LoadRankings(disk_cache::CacheAddr addr,
                               disk_cache::RankingsNode* rankings) {
  disk_cache::Addr address(addr);
  if (address.file_type() != disk_cache::RANKINGS)
    return false;

  disk_cache::MappedFile* file = block_files_.GetFile(address);
  if (!file)
    return false;

  disk_cache::StorageBlock<disk_cache::RankingsNode> rank_block(file, address);
  if (!rank_block.Load())
    return false;

  if (!rank_block.VerifyHash())
    printf("Self hash failed at 0x%x\n", addr);

  memcpy(rankings, rank_block.Data(), sizeof(*rankings));
  return true;
}

bool CacheDumper::HexDump(disk_cache::CacheAddr addr, std::string* out) {
  disk_cache::Addr address(addr);
  disk_cache::MappedFile* file = block_files_.GetFile(address);
  if (!file)
    return false;

  size_t size = address.num_blocks() * address.BlockSize();
  std::unique_ptr<char[]> buffer(new char[size]);

  size_t offset = address.start_block() * address.BlockSize() +
                  disk_cache::kBlockHeaderSize;
  if (!file->Read(buffer.get(), size, offset))
    return false;

  base::StringAppendF(out, "0x%x:\n", addr);
  net::ViewCacheHelper::HexDump(buffer.get(), size, out);
  return true;
}

std::string ToLocalTime(int64_t time_us) {
  base::Time time = base::Time::FromInternalValue(time_us);
  base::Time::Exploded e;
  time.LocalExplode(&e);
  return base::StringPrintf("%d/%d/%d %d:%d:%d.%d", e.year, e.month,
                            e.day_of_month, e.hour, e.minute, e.second,
                            e.millisecond);
}

void DumpEntry(disk_cache::CacheAddr addr,
               const disk_cache::EntryStore& entry,
               bool verbose) {
  std::string key;
  static bool full_key =
      base::CommandLine::ForCurrentProcess()->HasSwitch("full-key");
  if (!entry.long_key) {
    key = std::string(entry.key, std::min(static_cast<size_t>(entry.key_len),
                                          sizeof(entry.key)));
    if (entry.key_len > 90 && !full_key)
      key.resize(90);
  }

  printf("Entry at 0x%x\n", addr);
  printf("rankings: 0x%x\n", entry.rankings_node);
  printf("key length: %d\n", entry.key_len);
  printf("key: \"%s\"\n", key.c_str());

  if (verbose) {
    printf("key addr: 0x%x\n", entry.long_key);
    printf("hash: 0x%x\n", entry.hash);
    printf("next entry: 0x%x\n", entry.next);
    printf("reuse count: %d\n", entry.reuse_count);
    printf("refetch count: %d\n", entry.refetch_count);
    printf("state: %d\n", entry.state);
    printf("creation: %s\n", ToLocalTime(entry.creation_time).c_str());
    for (int i = 0; i < 4; i++) {
      printf("data size %d: %d\n", i, entry.data_size[i]);
      printf("data addr %d: 0x%x\n", i, entry.data_addr[i]);
    }
    printf("----------\n\n");
  }
}

void DumpRankings(disk_cache::CacheAddr addr,
                  const disk_cache::RankingsNode& rankings,
                  bool verbose) {
  printf("Rankings at 0x%x\n", addr);
  printf("next: 0x%x\n", rankings.next);
  printf("prev: 0x%x\n", rankings.prev);
  printf("entry: 0x%x\n", rankings.contents);

  if (verbose) {
    printf("dirty: %d\n", rankings.dirty);
    if (rankings.last_used != rankings.last_modified)
      printf("used: %s\n", ToLocalTime(rankings.last_used).c_str());
    printf("modified: %s\n", ToLocalTime(rankings.last_modified).c_str());
    printf("hash: 0x%x\n", rankings.self_hash);
    printf("----------\n\n");
  } else {
    printf("\n");
  }
}

void PrintCSVHeader() {
  printf(
      "entry,rankings,next,prev,rank-contents,chain,reuse,key,"
      "d0,d1,d2,d3\n");
}

void DumpCSV(disk_cache::CacheAddr addr,
             const disk_cache::EntryStore& entry,
             const disk_cache::RankingsNode& rankings) {
  printf("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x\n", addr,
         entry.rankings_node, rankings.next, rankings.prev, rankings.contents,
         entry.next, entry.reuse_count, entry.long_key, entry.data_addr[0],
         entry.data_addr[1], entry.data_addr[2], entry.data_addr[3]);

  if (addr != rankings.contents)
    printf("Broken entry\n");
}

bool CanDump(disk_cache::CacheAddr addr) {
  disk_cache::Addr address(addr);
  return address.is_initialized() && address.is_block_file();
}

}  // namespace.

// -----------------------------------------------------------------------

int GetMajorVersion(const base::FilePath& input_path) {
  base::FilePath index_name(input_path.Append(kIndexName));

  int version = GetMajorVersionFromFile(index_name);
  if (!version)
    return 0;

  base::FilePath data_name(input_path.Append(FILE_PATH_LITERAL("data_0")));
  if (version != GetMajorVersionFromFile(data_name))
    return 0;

  data_name = input_path.Append(FILE_PATH_LITERAL("data_1"));
  if (version != GetMajorVersionFromFile(data_name))
    return 0;

  data_name = input_path.Append(FILE_PATH_LITERAL("data_2"));
  if (version != GetMajorVersionFromFile(data_name))
    return 0;

  data_name = input_path.Append(FILE_PATH_LITERAL("data_3"));
  if (version != GetMajorVersionFromFile(data_name))
    return 0;

  return version;
}

// Dumps the headers of all files.
int DumpHeaders(const base::FilePath& input_path) {
  base::FilePath index_name(input_path.Append(kIndexName));
  disk_cache::CacheAddr stats_addr = 0;
  DumpIndexHeader(index_name, &stats_addr);

  base::FileEnumerator iter(input_path, false,
                            base::FileEnumerator::FILES,
                            FILE_PATH_LITERAL("data_*"));
  for (base::FilePath file = iter.Next(); !file.empty(); file = iter.Next())
    DumpBlockHeader(file);

  DumpStats(input_path, stats_addr);
  return 0;
}

// Dumps all entries from the cache.
int DumpContents(const base::FilePath& input_path) {
  bool print_csv = base::CommandLine::ForCurrentProcess()->HasSwitch("csv");
  if (!print_csv)
    DumpIndexHeader(input_path.Append(kIndexName), nullptr);

  // We need a task executor, although we really don't run any task.
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  CacheDumper dumper(input_path);
  if (!dumper.Init())
    return -1;

  if (print_csv)
    PrintCSVHeader();

  disk_cache::EntryStore entry;
  disk_cache::CacheAddr addr;
  bool verbose = base::CommandLine::ForCurrentProcess()->HasSwitch("v");
  while (dumper.GetEntry(&entry, &addr)) {
    if (!print_csv)
      DumpEntry(addr, entry, verbose);
    disk_cache::RankingsNode rankings;
    if (!dumper.LoadRankings(entry.rankings_node, &rankings))
      continue;

    if (print_csv)
      DumpCSV(addr, entry, rankings);
    else
      DumpRankings(entry.rankings_node, rankings, verbose);
  }

  printf("Done.\n");

  return 0;
}

int DumpLists(const base::FilePath& input_path) {
  base::FilePath index_name(input_path.Append(kIndexName));
  disk_cache::IndexHeader header;
  if (!ReadHeader(index_name, reinterpret_cast<char*>(&header), sizeof(header)))
    return -1;

  // We need a task executor, although we really don't run any task.
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  CacheDumper dumper(input_path);
  if (!dumper.Init())
    return -1;

  printf("list, addr,      next,       prev,       entry\n");

  const int kMaxLength = 1 * 1000 * 1000;
  for (int i = 0; i < 5; i++) {
    int32_t size = header.lru.sizes[i];
    if (size < 0 || size > kMaxLength) {
      printf("Wrong size %d\n", size);
      size = kMaxLength;
    }

    disk_cache::CacheAddr addr = header.lru.tails[i];
    int count = 0;
    for (; size && addr; size--) {
      count++;
      disk_cache::RankingsNode rankings;
      if (!dumper.LoadRankings(addr, &rankings)) {
        printf("Failed to load node at 0x%x\n", addr);
        break;
      }
      printf("%d, 0x%x, 0x%x, 0x%x, 0x%x\n", i, addr, rankings.next,
             rankings.prev, rankings.contents);

      if (rankings.prev == addr)
        break;

      addr = rankings.prev;
    }
    printf("%d nodes found, %d reported\n", count, header.lru.sizes[i]);
  }

  printf("Done.\n");
  return 0;
}

int DumpEntryAt(const base::FilePath& input_path, const std::string& at) {
  disk_cache::CacheAddr addr;
  if (!base::HexStringToUInt(at, &addr))
    return -1;

  if (!CanDump(addr))
    return -1;

  base::FilePath index_name(input_path.Append(kIndexName));
  disk_cache::IndexHeader header;
  if (!ReadHeader(index_name, reinterpret_cast<char*>(&header), sizeof(header)))
    return -1;

  // We need a task executor, although we really don't run any task.
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  CacheDumper dumper(input_path);
  if (!dumper.Init())
    return -1;

  disk_cache::CacheAddr entry_addr = 0;
  disk_cache::CacheAddr rankings_addr = 0;
  disk_cache::Addr address(addr);

  disk_cache::RankingsNode rankings;
  if (address.file_type() == disk_cache::RANKINGS) {
    if (dumper.LoadRankings(addr, &rankings)) {
      rankings_addr = addr;
      addr = rankings.contents;
      address = disk_cache::Addr(addr);
    }
  }

  disk_cache::EntryStore entry = {};
  if (address.file_type() == disk_cache::BLOCK_256 &&
      dumper.LoadEntry(addr, &entry)) {
    entry_addr = addr;
    DumpEntry(addr, entry, true);
    if (!rankings_addr && dumper.LoadRankings(entry.rankings_node, &rankings))
      rankings_addr = entry.rankings_node;
  }

  bool verbose = base::CommandLine::ForCurrentProcess()->HasSwitch("v");

  std::string hex_dump;
  if (!rankings_addr || verbose)
    dumper.HexDump(addr, &hex_dump);

  if (rankings_addr)
    DumpRankings(rankings_addr, rankings, true);

  if (entry_addr && verbose) {
    if (entry.long_key && CanDump(entry.long_key))
      dumper.HexDump(entry.long_key, &hex_dump);

    for (int i = 0; i < 4; i++) {
      if (entry.data_addr[i] && CanDump(entry.data_addr[i]))
        dumper.HexDump(entry.data_addr[i], &hex_dump);
    }
  }

  printf("%s\n", hex_dump.c_str());
  printf("Done.\n");
  return 0;
}

int DumpAllocation(const base::FilePath& file) {
  disk_cache::BlockFileHeader header;
  if (!ReadHeader(file, reinterpret_cast<char*>(&header), sizeof(header)))
    return -1;

  std::string out;
  net::ViewCacheHelper::HexDump(reinterpret_cast<char*>(&header.allocation_map),
                                sizeof(header.allocation_map), &out);
  printf("%s\n", out.c_str());
  return 0;
}
