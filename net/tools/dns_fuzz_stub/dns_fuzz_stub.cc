// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_util.h"
#include "net/dns/public/dns_protocol.h"

namespace {

void CrashDoubleFree(void) {
  // Cause memory corruption detectors to notice a double-free
  void *p = malloc(1);
  LOG(INFO) << "Allocated p=" << p << ".  Double-freeing...";
  free(p);
  free(p);
}

void CrashNullPointerDereference(void) {
  // Cause the program to segfault with a NULL pointer dereference
  int* p = nullptr;
  *p = 0;
}

bool FitsUint8(int num) {
  return (num >= 0) && (num <= std::numeric_limits<uint8_t>::max());
}

bool FitsUint16(int num) {
  return (num >= 0) && (num <= std::numeric_limits<uint16_t>::max());
}

bool ReadTestCase(const char* filename,
                  uint16_t* id,
                  std::string* qname,
                  uint16_t* qtype,
                  std::vector<char>* resp_buf,
                  bool* crash_test) {
  base::FilePath filepath = base::FilePath::FromUTF8Unsafe(filename);

  std::string json;
  if (!base::ReadFileToString(filepath, &json)) {
    LOG(ERROR) << filename << ": couldn't read file.";
    return false;
  }

  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(json);
  if (!value.get()) {
    LOG(ERROR) << filename << ": couldn't parse JSON.";
    return false;
  }

  base::DictionaryValue* dict;
  if (!value->GetAsDictionary(&dict)) {
    LOG(ERROR) << filename << ": test case is not a dictionary.";
    return false;
  }

  *crash_test = dict->HasKey("crash_test");
  if (*crash_test) {
    LOG(INFO) << filename << ": crash_test is set!";
    return true;
  }

  int id_int;
  if (!dict->GetInteger("id", &id_int)) {
    LOG(ERROR) << filename << ": id is missing or not an integer.";
    return false;
  }
  if (!FitsUint16(id_int)) {
    LOG(ERROR) << filename << ": id is out of range.";
    return false;
  }
  *id = static_cast<uint16_t>(id_int);

  if (!dict->GetStringASCII("qname", qname)) {
    LOG(ERROR) << filename << ": qname is missing or not a string.";
    return false;
  }

  int qtype_int;
  if (!dict->GetInteger("qtype", &qtype_int)) {
    LOG(ERROR) << filename << ": qtype is missing or not an integer.";
    return false;
  }
  if (!FitsUint16(qtype_int)) {
    LOG(ERROR) << filename << ": qtype is out of range.";
    return false;
  }
  *qtype = static_cast<uint16_t>(qtype_int);

  base::ListValue* resp_list;
  if (!dict->GetList("response", &resp_list)) {
    LOG(ERROR) << filename << ": response is missing or not a list.";
    return false;
  }

  size_t resp_size = resp_list->GetSize();
  resp_buf->clear();
  resp_buf->reserve(resp_size);
  for (size_t i = 0; i < resp_size; i++) {
    int resp_byte_int;
    if ((!resp_list->GetInteger(i, &resp_byte_int))) {
      LOG(ERROR) << filename << ": response[" << i << "] is not an integer.";
      return false;
    }
    if (!FitsUint8(resp_byte_int)) {
      LOG(ERROR) << filename << ": response[" << i << "] is out of range.";
      return false;
    }
    resp_buf->push_back(static_cast<char>(resp_byte_int));
  }
  DCHECK(resp_buf->size() == resp_size);

  LOG(INFO) << "Query: id=" << id_int << ", "
            << "qname=" << *qname << ", "
            << "qtype=" << qtype_int << ", "
            << "resp_size=" << resp_size;

  return true;
}

void RunTestCase(uint16_t id,
                 std::string& qname,
                 uint16_t qtype,
                 std::vector<char>& resp_buf) {
  net::DnsQuery query(id, qname, qtype);
  net::DnsResponse response;
  std::copy(resp_buf.begin(), resp_buf.end(), response.io_buffer()->data());

  if (!response.InitParse(resp_buf.size(), query)) {
    LOG(INFO) << "InitParse failed.";
    return;
  }

  net::AddressList address_list;
  base::TimeDelta ttl;
  net::DnsResponse::Result result = response.ParseToAddressList(
      &address_list, &ttl);
  if (result != net::DnsResponse::DNS_PARSE_OK) {
    LOG(INFO) << "ParseToAddressList failed: " << result;
    return;
  }

  // Print the response in one compact line.
  std::stringstream result_line;
  result_line << "Response: address_list={ ";
  for (unsigned int i = 0; i < address_list.size(); i++)
    result_line << address_list[i].ToString() << " ";
  result_line << "}, ttl=" << ttl.InSeconds() << "s";

  LOG(INFO) << result_line.str();
}

bool ReadAndRunTestCase(const char* filename) {
  uint16_t id = 0;
  std::string qname;
  uint16_t qtype = 0;
  std::vector<char> resp_buf;
  bool crash_test = false;

  LOG(INFO) << "Test case: " << filename;

  // ReadTestCase will print a useful error message if it fails.
  if (!ReadTestCase(filename, &id, &qname, &qtype, &resp_buf, &crash_test))
    return false;

  if (crash_test) {
    LOG(INFO) << "Crashing.";
    CrashDoubleFree();
    // if we're not running under a memory corruption detector, that
    // might not have worked
    CrashNullPointerDereference();
    NOTREACHED();
    return true;
  }

  std::string qname_dns;
  if (!net::DNSDomainFromDot(qname, &qname_dns)) {
    LOG(ERROR) << filename << ": DNSDomainFromDot(" << qname << ") failed.";
    return false;
  }

  RunTestCase(id, qname_dns, qtype, resp_buf);

  return true;
}

}  // anonymous namespace

int main(int argc, char** argv) {
  int ret = 0;

  for (int i = 1; i < argc; i++)
    if (!ReadAndRunTestCase(argv[i]))
      ret = 2;

  // Cluster-Fuzz likes "#EOF" as the last line of output to help distinguish
  // successful runs from crashes.
  printf("#EOF\n");

  return ret;
}
