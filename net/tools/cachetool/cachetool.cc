// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <memory>
#include <unordered_map>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/http/http_cache.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

using disk_cache::Backend;
using disk_cache::Entry;
using disk_cache::EntryResult;

namespace {

struct EntryData {
  std::string url;
  std::string mime_type;
  int size;
};

constexpr int kResponseInfoIndex = 0;
constexpr int kResponseContentIndex = 1;

const char* const kCommandNames[] = {
    "stop",          "get_size",   "list_keys",          "get_stream",
    "delete_stream", "delete_key", "update_raw_headers", "list_dups",
};

// Prints the command line help.
void PrintHelp() {
  std::cout << "cachetool <cache_path> <cache_backend_type> <subcommand> "
            << std::endl
            << std::endl;
  std::cout << "Available cache backend types: simple, blockfile" << std::endl;
  std::cout << "Available subcommands:" << std::endl;
  std::cout << "  batch: Starts cachetool to process serialized commands "
            << "passed down by the standard input and return commands output "
            << "in the stdout until the stop command is received." << std::endl;
  std::cout << "  delete_key <key>: Delete key from cache." << std::endl;
  std::cout << "  delete_stream <key> <index>: Delete a particular stream of a"
            << " given key." << std::endl;
  std::cout << "  get_size: Calculate the total size of the cache in bytes."
            << std::endl;
  std::cout << "  get_stream <key> <index>: Print a particular stream for a"
            << " given key." << std::endl;
  std::cout << "  list_keys: List all keys in the cache." << std::endl;
  std::cout << "  list_dups: List all resources with duplicate bodies in the "
            << "cache." << std::endl;
  std::cout << "  update_raw_headers <key>: Update stdin as the key's raw "
            << "response headers." << std::endl;
  std::cout << "  stop: Verify that the cache can be opened and return, "
            << "confirming the cache exists and is of the right type."
            << std::endl;
  std::cout << "Expected values of <index> are:" << std::endl;
  std::cout << "  0 (HTTP response headers)" << std::endl;
  std::cout << "  1 (transport encoded content)" << std::endl;
  std::cout << "  2 (compiled content)" << std::endl;
}

// Generic command input/output.
class CommandMarshal {
 public:
  explicit CommandMarshal(Backend* cache_backend)
      : command_failed_(false), cache_backend_(cache_backend) {}
  virtual ~CommandMarshal() = default;

  // Reads the next command's name to execute.
  virtual std::string ReadCommandName() = 0;

  // Reads the next parameter as an integer.
  virtual int ReadInt() = 0;

  // Reads the next parameter as stream index.
  int ReadStreamIndex() {
    if (has_failed())
      return -1;
    int index = ReadInt();
    if (index < 0 || index > 2) {
      ReturnFailure("Invalid stream index.");
      return -1;
    }
    return index;
  }

  // Reads the next parameter as a string.
  virtual std::string ReadString() = 0;

  // Reads the next parameter from stdin as string.
  virtual std::string ReadBufferedString() = 0;

  // Communicates back an integer.
  virtual void ReturnInt(int integer) = 0;

  // Communicates back a 64-bit integer.
  virtual void ReturnInt64(int64_t integer) = 0;

  // Communicates back a string.
  virtual void ReturnString(const std::string& string) = 0;

  // Communicates back a buffer.
  virtual void ReturnBuffer(net::GrowableIOBuffer* buffer) = 0;

  // Communicates back command failure.
  virtual void ReturnFailure(const std::string& error_msg) = 0;

  // Communicates back command success.
  virtual void ReturnSuccess() { DCHECK(!command_failed_); }

  // Returns whether the command has failed.
  inline bool has_failed() { return command_failed_; }

  // Returns the opened cache backend.
  Backend* cache_backend() { return cache_backend_; }

 protected:
  bool command_failed_;
  Backend* const cache_backend_;
};

// Command line input/output that is user readable.
class ProgramArgumentCommandMarshal final : public CommandMarshal {
 public:
  ProgramArgumentCommandMarshal(Backend* cache_backend,
                                base::CommandLine::StringVector args)
      : CommandMarshal(cache_backend), command_line_args_(args), args_id_(0) {}

  // Implements CommandMarshal.
  std::string ReadCommandName() override {
    if (args_id_ == 0)
      return ReadString();
    else if (args_id_ == command_line_args_.size())
      return "stop";
    else if (!has_failed())
      ReturnFailure("Command line arguments too long.");
    return "";
  }

  // Implements CommandMarshal.
  int ReadInt() override {
    std::string integer_str = ReadString();
    int integer = -1;
    if (!base::StringToInt(integer_str, &integer)) {
      ReturnFailure("Couldn't parse integer.");
      return 0;
    }
    return integer;
  }

  // Implements CommandMarshal.
  std::string ReadString() override {
    if (args_id_ < command_line_args_.size())
      return command_line_args_[args_id_++];
    if (!has_failed())
      ReturnFailure("Command line arguments to short.");
    return "";
  }

  // Implements CommandMarshal.
  std::string ReadBufferedString() override {
    std::ostringstream raw_headers_stream;
    for (std::string line; std::getline(std::cin, line);)
      raw_headers_stream << line << std::endl;
    return raw_headers_stream.str();
  }

  // Implements CommandMarshal.
  void ReturnInt(int integer) override {
    DCHECK(!has_failed());
    std::cout << integer << std::endl;
  }

  // Implements CommandMarshal.
  void ReturnInt64(int64_t integer) override {
    DCHECK(!has_failed());
    std::cout << integer << std::endl;
  }

  // Implements CommandMarshal.
  void ReturnString(const std::string& string) override {
    DCHECK(!has_failed());
    std::cout << string << std::endl;
  }

  // Implements CommandMarshal.
  void ReturnBuffer(net::GrowableIOBuffer* buffer) override {
    DCHECK(!has_failed());
    std::cout.write(buffer->StartOfBuffer(), buffer->offset());
  }

  // Implements CommandMarshal.
  void ReturnFailure(const std::string& error_msg) override {
    DCHECK(!has_failed());
    std::cerr << error_msg << std::endl;
    command_failed_ = true;
  }

 private:
  const base::CommandLine::StringVector command_line_args_;
  size_t args_id_;
};

// Online command input/output that receives pickled commands from stdin and
// returns their results back in stdout. Send the stop command to properly exit
// cachetool's main loop.
class StreamCommandMarshal final : public CommandMarshal {
 public:
  explicit StreamCommandMarshal(Backend* cache_backend)
      : CommandMarshal(cache_backend) {}

  // Implements CommandMarshal.
  std::string ReadCommandName() override {
    if (has_failed())
      return "";
    std::cout.flush();
    size_t command_id = static_cast<size_t>(std::cin.get());
    if (command_id >= base::size(kCommandNames)) {
      ReturnFailure("Unknown command.");
      return "";
    }
    return kCommandNames[command_id];
  }

  // Implements CommandMarshal.
  int ReadInt() override {
    if (has_failed())
      return -1;
    int integer = -1;
    std::cin.read(reinterpret_cast<char*>(&integer), sizeof(integer));
    return integer;
  }

  // Implements CommandMarshal.
  std::string ReadString() override {
    if (has_failed())
      return "";
    int string_size = ReadInt();
    if (string_size <= 0) {
      if (string_size < 0)
        ReturnFailure("Size of string is negative.");
      return "";
    }
    std::vector<char> tmp_buffer(string_size + 1);
    std::cin.read(tmp_buffer.data(), string_size);
    tmp_buffer[string_size] = 0;
    return std::string(tmp_buffer.data(), string_size);
  }

  // Implements CommandMarshal.
  std::string ReadBufferedString() override { return ReadString(); }

  // Implements CommandMarshal.
  void ReturnInt(int integer) override {
    DCHECK(!command_failed_);
    std::cout.write(reinterpret_cast<char*>(&integer), sizeof(integer));
  }

  // Implements CommandMarshal.
  void ReturnInt64(int64_t integer) override {
    DCHECK(!has_failed());
    std::cout.write(reinterpret_cast<char*>(&integer), sizeof(integer));
  }

  // Implements CommandMarshal.
  void ReturnString(const std::string& string) override {
    ReturnInt(string.size());
    std::cout.write(string.c_str(), string.size());
  }

  // Implements CommandMarshal.
  void ReturnBuffer(net::GrowableIOBuffer* buffer) override {
    ReturnInt(buffer->offset());
    std::cout.write(buffer->StartOfBuffer(), buffer->offset());
  }

  // Implements CommandMarshal.
  void ReturnFailure(const std::string& error_msg) override {
    ReturnString(error_msg);
    command_failed_ = true;
  }

  // Implements CommandMarshal.
  void ReturnSuccess() override { ReturnInt(0); }
};

// Gets the cache's size.
void GetSize(CommandMarshal* command_marshal) {
  net::TestInt64CompletionCallback cb;
  int64_t rv = command_marshal->cache_backend()->CalculateSizeOfAllEntries(
      cb.callback());
  rv = cb.GetResult(rv);
  if (rv < 0)
    return command_marshal->ReturnFailure("Couldn't get cache size.");
  command_marshal->ReturnSuccess();
  command_marshal->ReturnInt64(rv);
}

// Prints all of a cache's keys to stdout.
bool ListKeys(CommandMarshal* command_marshal) {
  std::unique_ptr<Backend::Iterator> entry_iterator =
      command_marshal->cache_backend()->CreateIterator();
  TestEntryResultCompletionCallback cb;
  EntryResult result = entry_iterator->OpenNextEntry(cb.callback());
  command_marshal->ReturnSuccess();
  while ((result = cb.GetResult(std::move(result))).net_error() == net::OK) {
    Entry* entry = result.ReleaseEntry();
    std::string url = entry->GetKey();
    command_marshal->ReturnString(url);
    entry->Close();
    result = entry_iterator->OpenNextEntry(cb.callback());
  }
  command_marshal->ReturnString("");
  return true;
}

bool GetResponseInfoForEntry(disk_cache::Entry* entry,
                             net::HttpResponseInfo* response_info) {
  int size = entry->GetDataSize(kResponseInfoIndex);
  if (size == 0)
    return false;
  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(size);
  net::TestCompletionCallback cb;

  int bytes_read = 0;
  while (true) {
    int rv = entry->ReadData(kResponseInfoIndex, bytes_read, buffer.get(), size,
                             cb.callback());
    rv = cb.GetResult(rv);
    if (rv < 0) {
      entry->Close();
      return false;
    }

    if (rv == 0) {
      bool truncated_response_info = false;
      if (!net::HttpCache::ParseResponseInfo(
              buffer->data(), size, response_info, &truncated_response_info)) {
        return false;
      }
      return !truncated_response_info;
    }

    bytes_read += rv;
  }

  NOTREACHED();
  return false;
}

std::string GetMD5ForResponseBody(disk_cache::Entry* entry) {
  if (entry->GetDataSize(kResponseContentIndex) == 0)
    return "";

  const int kInitBufferSize = 80 * 1024;
  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kInitBufferSize);
  net::TestCompletionCallback cb;

  base::MD5Context ctx;
  base::MD5Init(&ctx);

  int bytes_read = 0;
  while (true) {
    int rv = entry->ReadData(kResponseContentIndex, bytes_read, buffer.get(),
                             kInitBufferSize, cb.callback());
    rv = cb.GetResult(rv);
    if (rv < 0) {
      entry->Close();
      return "";
    }

    if (rv == 0) {
      base::MD5Digest digest;
      base::MD5Final(&digest, &ctx);
      return base::MD5DigestToBase16(digest);
    }

    bytes_read += rv;
    MD5Update(&ctx, base::StringPiece(buffer->data(), rv));
  }

  NOTREACHED();
  return "";
}

void ListDups(CommandMarshal* command_marshal) {
  std::unique_ptr<Backend::Iterator> entry_iterator =
      command_marshal->cache_backend()->CreateIterator();
  TestEntryResultCompletionCallback cb;
  disk_cache::EntryResult result = entry_iterator->OpenNextEntry(cb.callback());
  command_marshal->ReturnSuccess();

  std::unordered_map<std::string, std::vector<EntryData>> md5_entries;

  int total_entries = 0;

  while ((result = cb.GetResult(std::move(result))).net_error() == net::OK) {
    Entry* entry = result.ReleaseEntry();
    total_entries += 1;
    net::HttpResponseInfo response_info;
    if (!GetResponseInfoForEntry(entry, &response_info)) {
      entry->Close();
      entry = nullptr;
      result = entry_iterator->OpenNextEntry(cb.callback());
      continue;
    }

    std::string hash = GetMD5ForResponseBody(entry);
    if (hash.empty()) {
      // Sparse entries and empty bodies are skipped.
      entry->Close();
      entry = nullptr;
      result = entry_iterator->OpenNextEntry(cb.callback());
      continue;
    }

    EntryData entry_data;

    entry_data.url = entry->GetKey();
    entry_data.size = entry->GetDataSize(kResponseContentIndex);
    if (response_info.headers)
      response_info.headers->GetMimeType(&entry_data.mime_type);

    auto iter = md5_entries.find(hash);
    if (iter == md5_entries.end())
      md5_entries.insert(
          std::make_pair(hash, std::vector<EntryData>{entry_data}));
    else
      iter->second.push_back(entry_data);

    entry->Close();
    entry = nullptr;
    result = entry_iterator->OpenNextEntry(cb.callback());
  }

  // Print the duplicates and collect stats.
  int total_duped_entries = 0;
  int64_t total_duped_bytes = 0u;
  for (const auto& hash_and_entries : md5_entries) {
    if (hash_and_entries.second.size() == 1)
      continue;

    int dups = hash_and_entries.second.size() - 1;
    total_duped_entries += dups;
    total_duped_bytes += hash_and_entries.second[0].size * dups;

    for (const auto& entry : hash_and_entries.second) {
      std::string out = base::StringPrintf(
          "%d, %s, %s", entry.size, entry.url.c_str(), entry.mime_type.c_str());
      command_marshal->ReturnString(out);
    }
  }

  // Print the stats.
  net::TestInt64CompletionCallback size_cb;
  int64_t rv = command_marshal->cache_backend()->CalculateSizeOfAllEntries(
      size_cb.callback());
  rv = size_cb.GetResult(rv);
  LOG(ERROR) << "Wasted bytes = " << total_duped_bytes;
  LOG(ERROR) << "Wasted entries = " << total_duped_entries;
  LOG(ERROR) << "Total entries = " << total_entries;
  LOG(ERROR) << "Cache size = " << rv;
  LOG(ERROR) << "Percentage of cache wasted = " << total_duped_bytes * 100 / rv;
}

// Gets a key's stream to a buffer.
scoped_refptr<net::GrowableIOBuffer> GetStreamForKeyBuffer(
    CommandMarshal* command_marshal,
    const std::string& key,
    int index) {
  DCHECK(!command_marshal->has_failed());

  TestEntryResultCompletionCallback cb_open;
  EntryResult result = command_marshal->cache_backend()->OpenEntry(
      key, net::HIGHEST, cb_open.callback());
  result = cb_open.GetResult(std::move(result));
  if (result.net_error() != net::OK) {
    command_marshal->ReturnFailure("Couldn't find key's entry.");
    return nullptr;
  }
  Entry* cache_entry = result.ReleaseEntry();

  const int kInitBufferSize = 8192;
  scoped_refptr<net::GrowableIOBuffer> buffer =
      base::MakeRefCounted<net::GrowableIOBuffer>();
  buffer->SetCapacity(kInitBufferSize);
  net::TestCompletionCallback cb;
  while (true) {
    int rv = cache_entry->ReadData(index, buffer->offset(), buffer.get(),
                                   buffer->capacity() - buffer->offset(),
                                   cb.callback());
    rv = cb.GetResult(rv);
    if (rv < 0) {
      cache_entry->Close();
      command_marshal->ReturnFailure("Stream read error.");
      return nullptr;
    }
    buffer->set_offset(buffer->offset() + rv);
    if (rv == 0)
      break;
    buffer->SetCapacity(buffer->offset() * 2);
  }
  cache_entry->Close();
  return buffer;
}

// Prints a key's stream to stdout.
void GetStreamForKey(CommandMarshal* command_marshal) {
  std::string key = command_marshal->ReadString();
  int index = command_marshal->ReadInt();
  if (command_marshal->has_failed())
    return;
  scoped_refptr<net::GrowableIOBuffer> buffer(
      GetStreamForKeyBuffer(command_marshal, key, index));
  if (command_marshal->has_failed())
    return;
  if (index == kResponseInfoIndex) {
    net::HttpResponseInfo response_info;
    bool truncated_response_info = false;
    if (!net::HttpCache::ParseResponseInfo(buffer->StartOfBuffer(),
                                           buffer->offset(), &response_info,
                                           &truncated_response_info)) {
      // This can happen when reading data stored by content::CacheStorage.
      std::cerr << "WARNING: Returning empty response info for key: " << key
                << std::endl;
      command_marshal->ReturnSuccess();
      return command_marshal->ReturnString("");
    }
    if (truncated_response_info)
      std::cerr << "WARNING: Truncated HTTP response." << std::endl;
    command_marshal->ReturnSuccess();
    command_marshal->ReturnString(
        net::HttpUtil::ConvertHeadersBackToHTTPResponse(
            response_info.headers->raw_headers()));
  } else {
    command_marshal->ReturnSuccess();
    command_marshal->ReturnBuffer(buffer.get());
  }
}

// Sets stdin as the key's raw response headers.
void UpdateRawResponseHeaders(CommandMarshal* command_marshal) {
  std::string key = command_marshal->ReadString();
  std::string raw_headers = command_marshal->ReadBufferedString();
  if (command_marshal->has_failed())
    return;
  scoped_refptr<net::GrowableIOBuffer> buffer(
      GetStreamForKeyBuffer(command_marshal, key, kResponseInfoIndex));
  if (command_marshal->has_failed())
    return;
  net::HttpResponseInfo response_info;
  bool truncated_response_info = false;
  net::HttpCache::ParseResponseInfo(buffer->StartOfBuffer(), buffer->offset(),
                                    &response_info, &truncated_response_info);
  if (truncated_response_info)
    std::cerr << "WARNING: Truncated HTTP response." << std::endl;

  response_info.headers = new net::HttpResponseHeaders(raw_headers);
  scoped_refptr<net::PickledIOBuffer> data =
      base::MakeRefCounted<net::PickledIOBuffer>();
  response_info.Persist(data->pickle(), false, false);
  data->Done();

  TestEntryResultCompletionCallback cb_open;
  EntryResult result = command_marshal->cache_backend()->OpenEntry(
      key, net::HIGHEST, cb_open.callback());
  result = cb_open.GetResult(std::move(result));
  CHECK_EQ(result.net_error(), net::OK);
  Entry* cache_entry = result.ReleaseEntry();

  int data_len = data->pickle()->size();
  net::TestCompletionCallback cb;
  int rv = cache_entry->WriteData(kResponseInfoIndex, 0, data.get(), data_len,
                                  cb.callback(), true);
  if (cb.GetResult(rv) != data_len)
    return command_marshal->ReturnFailure("Couldn't write headers.");
  command_marshal->ReturnSuccess();
  cache_entry->Close();
}

// Deletes a specified key stream from the cache.
void DeleteStreamForKey(CommandMarshal* command_marshal) {
  std::string key = command_marshal->ReadString();
  int index = command_marshal->ReadInt();
  if (command_marshal->has_failed())
    return;

  TestEntryResultCompletionCallback cb_open;
  EntryResult result = command_marshal->cache_backend()->OpenEntry(
      key, net::HIGHEST, cb_open.callback());
  result = cb_open.GetResult(std::move(result));
  if (result.net_error() != net::OK)
    return command_marshal->ReturnFailure("Couldn't find key's entry.");
  Entry* cache_entry = result.ReleaseEntry();

  net::TestCompletionCallback cb;
  scoped_refptr<net::StringIOBuffer> buffer =
      base::MakeRefCounted<net::StringIOBuffer>("");
  int rv =
      cache_entry->WriteData(index, 0, buffer.get(), 0, cb.callback(), true);
  if (cb.GetResult(rv) != net::OK)
    return command_marshal->ReturnFailure("Couldn't delete key stream.");
  command_marshal->ReturnSuccess();
  cache_entry->Close();
}

// Deletes a specified key from the cache.
void DeleteKey(CommandMarshal* command_marshal) {
  std::string key = command_marshal->ReadString();
  if (command_marshal->has_failed())
    return;
  net::TestCompletionCallback cb;
  int rv = command_marshal->cache_backend()->DoomEntry(key, net::HIGHEST,
                                                       cb.callback());
  if (cb.GetResult(rv) != net::OK)
    command_marshal->ReturnFailure("Couldn't delete key.");
  else
    command_marshal->ReturnSuccess();
}

// Executes all command from the |command_marshal|.
bool ExecuteCommands(CommandMarshal* command_marshal) {
  while (!command_marshal->has_failed()) {
    std::string subcommand(command_marshal->ReadCommandName());
    if (command_marshal->has_failed())
      break;
    if (subcommand == "stop") {
      command_marshal->ReturnSuccess();
      return true;
    } else if (subcommand == "batch") {
      StreamCommandMarshal stream_command_marshal(
          command_marshal->cache_backend());
      return ExecuteCommands(&stream_command_marshal);
    } else if (subcommand == "delete_key") {
      DeleteKey(command_marshal);
    } else if (subcommand == "delete_stream") {
      DeleteStreamForKey(command_marshal);
    } else if (subcommand == "get_size") {
      GetSize(command_marshal);
    } else if (subcommand == "get_stream") {
      GetStreamForKey(command_marshal);
    } else if (subcommand == "list_keys") {
      ListKeys(command_marshal);
    } else if (subcommand == "update_raw_headers") {
      UpdateRawResponseHeaders(command_marshal);
    } else if (subcommand == "list_dups") {
      ListDups(command_marshal);
    } else {
      // The wrong subcommand is originated from the command line.
      command_marshal->ReturnFailure("Unknown command.");
      PrintHelp();
    }
  }
  return false;
}

}  // namespace

int main(int argc, char* argv[]) {
  base::AtExitManager at_exit_manager;
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  base::CommandLine::StringVector args = command_line.GetArgs();
  if (args.size() < 3U) {
    PrintHelp();
    return 1;
  }

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("cachetool");

  base::FilePath cache_path(args[0]);
  std::string cache_backend_type(args[1]);

  net::BackendType backend_type;
  if (cache_backend_type == "simple") {
    backend_type = net::CACHE_BACKEND_SIMPLE;
  } else if (cache_backend_type == "blockfile") {
    backend_type = net::CACHE_BACKEND_BLOCKFILE;
  } else {
    std::cerr << "Unknown cache type." << std::endl;
    PrintHelp();
    return 1;
  }

  std::unique_ptr<Backend> cache_backend;
  net::TestCompletionCallback cb;
  int rv = disk_cache::CreateCacheBackend(
      net::DISK_CACHE, backend_type, cache_path, INT_MAX,
      disk_cache::ResetHandling::kNeverReset, nullptr, &cache_backend,
      cb.callback());
  if (cb.GetResult(rv) != net::OK) {
    std::cerr << "Invalid cache." << std::endl;
    return 1;
  }

  ProgramArgumentCommandMarshal program_argument_marshal(
      cache_backend.get(),
      base::CommandLine::StringVector(args.begin() + 2, args.end()));
  bool successful_commands = ExecuteCommands(&program_argument_marshal);

  base::RunLoop().RunUntilIdle();
  cache_backend = nullptr;
  disk_cache::FlushCacheThreadForTesting();
  base::RunLoop().RunUntilIdle();
  return !successful_commands;
}
