// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/record_rdata.h"

#include <algorithm>
#include <memory>

#include "base/big_endian.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_test_util.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

using ::testing::ElementsAreArray;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::SizeIs;

base::StringPiece MakeStringPiece(const uint8_t* data, unsigned size) {
  const char* data_cc = reinterpret_cast<const char*>(data);
  return base::StringPiece(data_cc, size);
}

TEST(RecordRdataTest, ParseSrvRecord) {
  // These are just the rdata portions of the DNS records, rather than complete
  // records, but it works well enough for this test.

  const uint8_t
      record[] =
          {
              0x00, 0x01, 0x00, 0x02, 0x00, 0x50, 0x03, 'w',  'w',
              'w',  0x06, 'g',  'o',  'o',  'g',  'l',  'e',  0x03,
              'c',  'o',  'm',  0x00, 0x01, 0x01, 0x01, 0x02, 0x01,
              0x03, 0x04, 'w',  'w',  'w',  '2',  0xc0, 0x0a,  // Pointer to
                                                               // "google.com"
          };

  DnsRecordParser parser(record, sizeof(record), 0);
  const unsigned first_record_len = 22;
  base::StringPiece record1_strpiece = MakeStringPiece(
      record, first_record_len);
  base::StringPiece record2_strpiece = MakeStringPiece(
      record + first_record_len, sizeof(record) - first_record_len);

  std::unique_ptr<SrvRecordRdata> record1_obj =
      SrvRecordRdata::Create(record1_strpiece, parser);
  ASSERT_TRUE(record1_obj != nullptr);
  ASSERT_EQ(1, record1_obj->priority());
  ASSERT_EQ(2, record1_obj->weight());
  ASSERT_EQ(80, record1_obj->port());

  ASSERT_EQ("www.google.com", record1_obj->target());

  std::unique_ptr<SrvRecordRdata> record2_obj =
      SrvRecordRdata::Create(record2_strpiece, parser);
  ASSERT_TRUE(record2_obj != nullptr);
  ASSERT_EQ(257, record2_obj->priority());
  ASSERT_EQ(258, record2_obj->weight());
  ASSERT_EQ(259, record2_obj->port());

  ASSERT_EQ("www2.google.com", record2_obj->target());

  ASSERT_TRUE(record1_obj->IsEqual(record1_obj.get()));
  ASSERT_FALSE(record1_obj->IsEqual(record2_obj.get()));
}

TEST(RecordRdataTest, ParseARecord) {
  // These are just the rdata portions of the DNS records, rather than complete
  // records, but it works well enough for this test.

  const uint8_t record[] = {
      0x7F, 0x00, 0x00, 0x01  // 127.0.0.1
  };

  DnsRecordParser parser(record, sizeof(record), 0);
  base::StringPiece record_strpiece = MakeStringPiece(record, sizeof(record));

  std::unique_ptr<ARecordRdata> record_obj =
      ARecordRdata::Create(record_strpiece, parser);
  ASSERT_TRUE(record_obj != nullptr);

  ASSERT_EQ("127.0.0.1", record_obj->address().ToString());

  ASSERT_TRUE(record_obj->IsEqual(record_obj.get()));
}

TEST(RecordRdataTest, ParseAAAARecord) {
  // These are just the rdata portions of the DNS records, rather than complete
  // records, but it works well enough for this test.

  const uint8_t record[] = {
      0x12, 0x34, 0x56, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09  // 1234:5678::9A
  };

  DnsRecordParser parser(record, sizeof(record), 0);
  base::StringPiece record_strpiece = MakeStringPiece(record, sizeof(record));

  std::unique_ptr<AAAARecordRdata> record_obj =
      AAAARecordRdata::Create(record_strpiece, parser);
  ASSERT_TRUE(record_obj != nullptr);

  ASSERT_EQ("1234:5678::9", record_obj->address().ToString());

  ASSERT_TRUE(record_obj->IsEqual(record_obj.get()));
}

TEST(RecordRdataTest, ParseCnameRecord) {
  // These are just the rdata portions of the DNS records, rather than complete
  // records, but it works well enough for this test.

  const uint8_t record[] = {0x03, 'w', 'w', 'w',  0x06, 'g', 'o', 'o',
                            'g',  'l', 'e', 0x03, 'c',  'o', 'm', 0x00};

  DnsRecordParser parser(record, sizeof(record), 0);
  base::StringPiece record_strpiece = MakeStringPiece(record, sizeof(record));

  std::unique_ptr<CnameRecordRdata> record_obj =
      CnameRecordRdata::Create(record_strpiece, parser);
  ASSERT_TRUE(record_obj != nullptr);

  ASSERT_EQ("www.google.com", record_obj->cname());

  ASSERT_TRUE(record_obj->IsEqual(record_obj.get()));
}

// Appends a well-formed ESNIKeys struct to the stream owned by "writer".
// Returns the length, in bytes, of this struct, or 0 on error.
//
// (There is no ambiguity in the return value because a well-formed
// ESNIKeys struct has positive length.)
void AppendWellFormedEsniKeys(base::BigEndianWriter* writer) {
  CHECK(writer);
  writer->WriteBytes(kWellFormedEsniKeys, kWellFormedEsniKeysSize);
}

// This helper checks |keys| against the well-formed sample ESNIKeys
// struct; it's necessary because we can't implicitly convert
// kWellFormedEsniKeys to a StringPiece (it's a byte array, not a
// null-terminated string).
void ExpectMatchesSampleKeys(base::StringPiece keys) {
  EXPECT_EQ(keys,
            base::StringPiece(kWellFormedEsniKeys, kWellFormedEsniKeysSize));
}

// Appends an IP address in network byte order, prepended by one byte
// containing its version number, to |*serialized_addresses|. Appends
// the corresponding IPAddress object to |*address_objects|.
void AppendAnotherIPAddress(std::vector<uint8_t>* serialized_addresses,
                            std::vector<IPAddress>* address_objects,
                            int ip_version) {
  CHECK(serialized_addresses);
  CHECK(address_objects);

  // To make the addresses vary, but in a deterministic manner, assign octets
  // in increasing order as they're requested, potentially eventually wrapping
  // to 0.
  static uint8_t next_octet;

  CHECK(ip_version == 4 || ip_version == 6);
  const int address_num_bytes = ip_version == 4 ? 4 : 16;

  std::vector<uint8_t> address_bytes;
  for (int i = 0; i < address_num_bytes; ++i)
    address_bytes.push_back(next_octet++);
  IPAddress address(address_bytes.data(), address_num_bytes);

  serialized_addresses->push_back(ip_version);
  serialized_addresses->insert(serialized_addresses->end(),
                               address_bytes.begin(), address_bytes.end());
  address_objects->push_back(address);
}

// Writes a dns_extensions ESNIRecord block containing given the address
// set to |writer|. This involves:
// - writing the 16-bit length prefix for the dns_extensions block
// - writing the 16-bit extension type (0x1001 "address_set")
// - writing the 16-bit length prefix for the address set extension
// - writing the extension itself
void AppendDnsExtensionsBlock(base::BigEndianWriter* writer,
                              const std::vector<uint8_t>& address_list) {
  CHECK(writer);
  // 2 bytes for the DNS extension type
  writer->WriteU16(4 +
                   address_list.size());  // length of the dns_extensions field
  writer->WriteU16(EsniRecordRdata::kAddressSetExtensionType);
  writer->WriteU16(address_list.size());  // length of the address set
  writer->WriteBytes(address_list.data(), address_list.size());
}

// Test parsing a well-formed ESNI record with no DNS extensions.
TEST(RecordRdataTest, ParseEsniRecordNoExtensions) {
  char record[10000] = {};
  base::BigEndianWriter writer(record, sizeof(record));
  AppendWellFormedEsniKeys(&writer);

  writer.WriteU16(0);  // dns_extensions length

  auto record_size = writer.ptr() - record;
  DnsRecordParser parser(record, record_size, 0 /* offset */);

  std::unique_ptr<EsniRecordRdata> record_obj =
      EsniRecordRdata::Create(std::string(record, record_size), parser);
  ASSERT_THAT(record_obj, NotNull());
  EXPECT_TRUE(record_obj->IsEqual(record_obj.get()));
  EXPECT_EQ(record_obj->esni_keys(),
            std::string(kWellFormedEsniKeys, kWellFormedEsniKeysSize));
  EXPECT_EQ(record_obj->Type(), dns_protocol::kExperimentalTypeEsniDraft4);
}

// Test parsing a well-formed ESNI record bearing an address_set extension
// containing a single IPv4 address.
TEST(RecordRdataTest, ParseEsniRecordOneIPv4Address) {
  // ESNI record:
  // well-formed ESNI keys
  // extensions length
  // extension
  char record[10000] = {};
  base::BigEndianWriter writer(record, sizeof(record));
  AppendWellFormedEsniKeys(&writer);

  std::vector<uint8_t> address_list;
  std::vector<IPAddress> addresses_for_validation;

  AppendAnotherIPAddress(&address_list, &addresses_for_validation,
                         4 /* ip_version */);

  AppendDnsExtensionsBlock(&writer, address_list);

  auto record_size = writer.ptr() - record;
  DnsRecordParser parser(record, record_size, 0 /* offset */);

  std::unique_ptr<EsniRecordRdata> record_obj =
      EsniRecordRdata::Create(std::string(record, record_size), parser);
  ASSERT_THAT(record_obj, NotNull());

  const auto& addresses = record_obj->addresses();

  EXPECT_EQ(addresses, addresses_for_validation);

  EXPECT_TRUE(record_obj->IsEqual(record_obj.get()));
  ExpectMatchesSampleKeys(record_obj->esni_keys());
}

// Test parsing a well-formed ESNI record bearing an address_set extension
// containing a single IPv6 address.
TEST(RecordRdataTest, ParseEsniRecordOneIPv6Address) {
  // ESNI record:
  // well-formed ESNI keys
  // extensions length
  // extension
  char record[10000] = {};
  base::BigEndianWriter writer(record, sizeof(record));
  AppendWellFormedEsniKeys(&writer);

  std::vector<uint8_t> address_list;
  std::vector<IPAddress> addresses_for_validation;

  AppendAnotherIPAddress(&address_list, &addresses_for_validation,
                         6 /* ip_version */);

  AppendDnsExtensionsBlock(&writer, address_list);

  auto record_size = writer.ptr() - record;
  DnsRecordParser parser(record, record_size, 0 /* offset */);

  std::unique_ptr<EsniRecordRdata> record_obj =
      EsniRecordRdata::Create(std::string(record, record_size), parser);
  ASSERT_THAT(record_obj, NotNull());

  const auto& addresses = record_obj->addresses();

  EXPECT_EQ(addresses, addresses_for_validation);

  EXPECT_TRUE(record_obj->IsEqual(record_obj.get()));
  ExpectMatchesSampleKeys(record_obj->esni_keys());
}

// Test parsing a well-formed ESNI record bearing an address_set extension
// containing several IPv4 and IPv6 addresses.
TEST(RecordRdataTest, ParseEsniRecordManyAddresses) {
  // ESNI record:
  // well-formed ESNI keys
  // extensions length
  // extension
  char record[10000] = {};
  base::BigEndianWriter writer(record, sizeof(record));
  AppendWellFormedEsniKeys(&writer);

  std::vector<uint8_t> address_list;
  std::vector<IPAddress> addresses_for_validation;

  for (int i = 0; i < 100; ++i)
    AppendAnotherIPAddress(&address_list, &addresses_for_validation,
                           (i % 3) ? 4 : 6 /* ip_version */);

  AppendDnsExtensionsBlock(&writer, address_list);

  auto record_size = writer.ptr() - record;
  DnsRecordParser parser(record, record_size, 0 /* offset */);

  std::unique_ptr<EsniRecordRdata> record_obj =
      EsniRecordRdata::Create(std::string(record, record_size), parser);
  ASSERT_THAT(record_obj, NotNull());

  const auto& addresses = record_obj->addresses();

  EXPECT_EQ(addresses, addresses_for_validation);

  EXPECT_TRUE(record_obj->IsEqual(record_obj.get()));
  ExpectMatchesSampleKeys(record_obj->esni_keys());
}

// Test that we correctly reject a record with an ill-formed ESNI keys field.
//
// This test makes sure that the //net-side record parser is able
// correctly to handle the case where an external ESNI keys validation
// subroutine reports that the keys are ill-formed; because this validation
// will eventually be performed by BoringSSL once the corresponding
// BSSL code lands, it's out of scope here to exercise the
// validation logic itself.
TEST(RecordRdataTest, EsniMalformedRecord_InvalidEsniKeys) {
  char record[10000] = {};
  base::BigEndianWriter writer(record, sizeof(record));

  // Oops! This otherwise well-formed ESNIKeys struct is missing its
  // final byte, and the reader contains no content after this incomplete
  // struct.
  const char ill_formed_esni_keys[] = {
      0xff, 0x3,  0x0,  0x0,  0x0,  0x24, 0x0,  0x1d, 0x0,  0x20,
      0xed, 0xed, 0xc8, 0x68, 0xc1, 0x71, 0xd6, 0x9e, 0xa9, 0xf0,
      0xa2, 0xc9, 0xf5, 0xa9, 0xdc, 0xcf, 0xf9, 0xb8, 0xed, 0x15,
      0x5c, 0xc4, 0x5a, 0xec, 0x6f, 0xb2, 0x86, 0x14, 0xb7, 0x71,
      0x1b, 0x7c, 0x0,  0x2,  0x13, 0x1,  0x1,  0x4,  0x0};
  writer.WriteBytes(ill_formed_esni_keys, sizeof(ill_formed_esni_keys));

  auto record_size = writer.ptr() - record;
  DnsRecordParser parser(record, record_size, 0 /* offset */);
  std::unique_ptr<EsniRecordRdata> record_obj =
      EsniRecordRdata::Create(std::string(record, record_size), parser);

  ASSERT_THAT(record_obj, IsNull());
}

// Test that an empty address_set extension is correctly accepted.
TEST(RecordRdataTest, ParseEsniRecord_EmptyAddressSet) {
  char record[10000] = {};
  base::BigEndianWriter writer(record, sizeof(record));
  AppendWellFormedEsniKeys(&writer);

  writer.WriteU16(4);  // length of the dns_extensions field
  writer.WriteU16(EsniRecordRdata::kAddressSetExtensionType);
  writer.WriteU16(0);  // length of the (empty) address_set extension

  auto record_size = writer.ptr() - record;
  DnsRecordParser parser(record, record_size, 0 /* offset */);
  std::unique_ptr<EsniRecordRdata> record_obj =
      EsniRecordRdata::Create(std::string(record, record_size), parser);

  ASSERT_THAT(record_obj, NotNull());
}

// Test that we correctly reject a record invalid due to having extra
// data within its dns_extensions block but after its last extension.
TEST(RecordRdataTest, EsniMalformedRecord_TrailingDataWithinDnsExtensions) {
  char record[10000] = {};
  base::BigEndianWriter writer(record, sizeof(record));
  AppendWellFormedEsniKeys(&writer);

  writer.WriteU16(5);  // length of the dns_extensions field
  writer.WriteU16(EsniRecordRdata::kAddressSetExtensionType);
  writer.WriteU16(0);  // length of the (empty) address_set extension

  // Pad the otherwise-valid extensions block with one byte of garbage.
  writer.WriteBytes(&"a", 1);

  auto record_size = writer.ptr() - record;
  DnsRecordParser parser(record, record_size, 0 /* offset */);
  std::unique_ptr<EsniRecordRdata> record_obj =
      EsniRecordRdata::Create(std::string(record, record_size), parser);

  ASSERT_THAT(record_obj, IsNull());
}

// Test that we correctly reject a record with two well-formed
// DNS extensions (only one extension of each type is permitted).
TEST(RecordRdataTest, EsniMalformedRecord_TooManyExtensions) {
  char record[10000] = {};
  base::BigEndianWriter writer(record, sizeof(record));
  AppendWellFormedEsniKeys(&writer);

  writer.WriteU16(8);  // length of the dns_extensions field
  writer.WriteU16(EsniRecordRdata::kAddressSetExtensionType);
  writer.WriteU16(0);  // length of the (empty) address_set extension
  // Write another (empty, but completely valid on its own) extension,
  // rendering the record invalid.
  writer.WriteU16(EsniRecordRdata::kAddressSetExtensionType);
  writer.WriteU16(0);

  auto record_size = writer.ptr() - record;
  DnsRecordParser parser(record, record_size, 0 /* offset */);
  std::unique_ptr<EsniRecordRdata> record_obj =
      EsniRecordRdata::Create(std::string(record, record_size), parser);

  ASSERT_THAT(record_obj, IsNull());
}

// Test that an ESNIRecord with an extension of invalid type
// is correctly rejected.
TEST(RecordRdataTest, EsniMalformedRecord_InvalidExtensionType) {
  char record[10000] = {};
  base::BigEndianWriter writer(record, sizeof(record));
  AppendWellFormedEsniKeys(&writer);

  // 2 bytes for the DNS extension type
  writer.WriteU16(2);       // length of the dns_extensions field
  writer.WriteU16(0xdead);  // invalid address type

  auto record_size = writer.ptr() - record;
  DnsRecordParser parser(record, record_size, 0 /* offset */);
  std::unique_ptr<EsniRecordRdata> record_obj =
      EsniRecordRdata::Create(std::string(record, record_size), parser);

  ASSERT_THAT(record_obj, IsNull());
}

// Test that an address_set extension missing a length field
// is correctly rejected.
TEST(RecordRdataTest, EsniMalformedRecord_MalformedAddressSetLength) {
  char record[10000] = {};
  base::BigEndianWriter writer(record, sizeof(record));
  AppendWellFormedEsniKeys(&writer);

  // 3 bytes: 2 for the DNS extension type, and one for our
  // too-short address_set length
  writer.WriteU16(3);  // length of the dns_extensions field
  writer.WriteU16(EsniRecordRdata::kAddressSetExtensionType);
  // oops! need two bytes for the address length
  writer.WriteU8(57);

  auto record_size = writer.ptr() - record;
  DnsRecordParser parser(record, record_size, 0 /* offset */);
  std::unique_ptr<EsniRecordRdata> record_obj =
      EsniRecordRdata::Create(std::string(record, record_size), parser);

  ASSERT_THAT(record_obj, IsNull());
}

// Test that an ESNI record with malformed dns_extensions length is
// correctly rejected.
TEST(RecordRdataTest, EsniMalformedRecord_MalformedDnsExtensionsLength) {
  char record[10000] = {};
  base::BigEndianWriter writer(record, sizeof(record));
  AppendWellFormedEsniKeys(&writer);

  // Oops! Length field of dns_extensions should be 2 bytes.
  writer.WriteU8(57);

  auto record_size = writer.ptr() - record;
  DnsRecordParser parser(record, record_size, 0 /* offset */);
  std::unique_ptr<EsniRecordRdata> record_obj =
      EsniRecordRdata::Create(std::string(record, record_size), parser);

  ASSERT_THAT(record_obj, IsNull());
}

// Test that an ESNI record with invalid dns_extensions length is
// correctly rejected.
TEST(RecordRdataTest, EsniMalformedRecord_BadDnsExtensionsLength) {
  char record[10000] = {};
  base::BigEndianWriter writer(record, sizeof(record));
  AppendWellFormedEsniKeys(&writer);

  // Length-prepend the dns_extensions field with value 5, even though
  // the extensions object will have length 4 (two U16's): this should
  // make the record be rejected as malformed.
  writer.WriteU16(5);
  writer.WriteU16(EsniRecordRdata::kAddressSetExtensionType);
  writer.WriteU16(0);  // length of the address_set extension

  auto record_size = writer.ptr() - record;
  DnsRecordParser parser(record, record_size, 0 /* offset */);
  std::unique_ptr<EsniRecordRdata> record_obj =
      EsniRecordRdata::Create(std::string(record, record_size), parser);

  ASSERT_THAT(record_obj, IsNull());
}

// Test that an ESNI record with invalid address_set extension length is
// correctly rejected.
TEST(RecordRdataTest, EsniMalformedRecord_BadAddressSetLength) {
  char record[10000] = {};
  base::BigEndianWriter writer(record, sizeof(record));
  AppendWellFormedEsniKeys(&writer);

  writer.WriteU16(4);  // 2 bytes for each of the U16s to be written
  writer.WriteU16(EsniRecordRdata::kAddressSetExtensionType);
  // Oops! Length-prepending the empty address_set field with the value 1.
  writer.WriteU16(1);

  auto record_size = writer.ptr() - record;
  DnsRecordParser parser(record, record_size, 0 /* offset */);
  std::unique_ptr<EsniRecordRdata> record_obj =
      EsniRecordRdata::Create(std::string(record, record_size), parser);

  ASSERT_THAT(record_obj, IsNull());
}

// Test that an ESNI record with an address_set entry of bad address
// type is correctly rejected.
TEST(RecordRdataTest, EsniMalformedRecord_InvalidAddressType) {
  char record[10000] = {};
  base::BigEndianWriter writer(record, sizeof(record));
  AppendWellFormedEsniKeys(&writer);

  writer.WriteU16(9);  // dns_extensions length: two U16's and a 5-byte address
  writer.WriteU16(EsniRecordRdata::kAddressSetExtensionType);

  std::vector<uint8_t> address_list;
  IPAddress ipv4;
  ASSERT_TRUE(net::ParseURLHostnameToAddress("192.168.1.1", &ipv4));
  address_list.push_back(5);  // Oops! "5" isn't a valid AddressType.
  std::copy(ipv4.bytes().begin(), ipv4.bytes().end(),
            std::back_inserter(address_list));

  writer.WriteU16(address_list.size());
  writer.WriteBytes(address_list.data(), address_list.size());

  auto record_size = writer.ptr() - record;
  DnsRecordParser parser(record, record_size, 0 /* offset */);
  std::unique_ptr<EsniRecordRdata> record_obj =
      EsniRecordRdata::Create(std::string(record, record_size), parser);

  ASSERT_THAT(record_obj, IsNull());
}

// Test that an ESNI record with an address_set entry of bad address
// type is correctly rejected.
TEST(RecordRdataTest, EsniMalformedRecord_NotEnoughAddressData_IPv4) {
  char record[10000] = {};
  base::BigEndianWriter writer(record, sizeof(record));
  AppendWellFormedEsniKeys(&writer);

  std::vector<uint8_t> address_list;
  std::vector<IPAddress> addresses_for_validation_unused;
  AppendAnotherIPAddress(&address_list, &addresses_for_validation_unused, 4);

  // dns_extensions length: 2 bytes for address type, 2 for address_set length
  // Subtract 1 because we're deliberately writing one byte too few for the
  // purposes of this test.
  writer.WriteU16(address_list.size() - 1 + 4);
  writer.WriteU16(EsniRecordRdata::kAddressSetExtensionType);
  writer.WriteU16(address_list.size() - 1);
  // oops! missing the last byte of our IPv4 address
  writer.WriteBytes(address_list.data(), address_list.size() - 1);

  auto record_size = writer.ptr() - record;
  DnsRecordParser parser(record, record_size, 0 /* offset */);
  std::unique_ptr<EsniRecordRdata> record_obj =
      EsniRecordRdata::Create(std::string(record, record_size), parser);

  ASSERT_THAT(record_obj, IsNull());
}

// Test that an ESNI record with an address_set entry of bad address
// type is correctly rejected.
TEST(RecordRdataTest, EsniMalformedRecord_NotEnoughAddressData_IPv6) {
  char record[10000] = {};
  base::BigEndianWriter writer(record, sizeof(record));
  AppendWellFormedEsniKeys(&writer);

  std::vector<uint8_t> address_list;
  std::vector<IPAddress> addresses_for_validation_unused;
  AppendAnotherIPAddress(&address_list, &addresses_for_validation_unused, 6);

  // dns_extensions length: 2 bytes for address type, 2 for address_set length
  // Subtract 1 because we're deliberately writing one byte too few for the
  // purposes of this test.
  writer.WriteU16(address_list.size() - 1 + 4);
  writer.WriteU16(EsniRecordRdata::kAddressSetExtensionType);
  writer.WriteU16(address_list.size() - 1);
  // oops! missing the last byte of our IPv6 address
  writer.WriteBytes(address_list.data(), address_list.size() - 1);

  auto record_size = writer.ptr() - record;
  DnsRecordParser parser(record, record_size, 0 /* offset */);
  std::unique_ptr<EsniRecordRdata> record_obj =
      EsniRecordRdata::Create(std::string(record, record_size), parser);

  ASSERT_THAT(record_obj, IsNull());
}

TEST(RecordRdataTest, ParsePtrRecord) {
  // These are just the rdata portions of the DNS records, rather than complete
  // records, but it works well enough for this test.

  const uint8_t record[] = {0x03, 'w', 'w', 'w',  0x06, 'g', 'o', 'o',
                            'g',  'l', 'e', 0x03, 'c',  'o', 'm', 0x00};

  DnsRecordParser parser(record, sizeof(record), 0);
  base::StringPiece record_strpiece = MakeStringPiece(record, sizeof(record));

  std::unique_ptr<PtrRecordRdata> record_obj =
      PtrRecordRdata::Create(record_strpiece, parser);
  ASSERT_TRUE(record_obj != nullptr);

  ASSERT_EQ("www.google.com", record_obj->ptrdomain());

  ASSERT_TRUE(record_obj->IsEqual(record_obj.get()));
}

TEST(RecordRdataTest, ParseTxtRecord) {
  // These are just the rdata portions of the DNS records, rather than complete
  // records, but it works well enough for this test.

  const uint8_t record[] = {0x03, 'w', 'w', 'w',  0x06, 'g', 'o', 'o',
                            'g',  'l', 'e', 0x03, 'c',  'o', 'm'};

  DnsRecordParser parser(record, sizeof(record), 0);
  base::StringPiece record_strpiece = MakeStringPiece(record, sizeof(record));

  std::unique_ptr<TxtRecordRdata> record_obj =
      TxtRecordRdata::Create(record_strpiece, parser);
  ASSERT_TRUE(record_obj != nullptr);

  std::vector<std::string> expected;
  expected.push_back("www");
  expected.push_back("google");
  expected.push_back("com");

  ASSERT_EQ(expected, record_obj->texts());

  ASSERT_TRUE(record_obj->IsEqual(record_obj.get()));
}

TEST(RecordRdataTest, ParseNsecRecord) {
  // These are just the rdata portions of the DNS records, rather than complete
  // records, but it works well enough for this test.

  const uint8_t record[] = {0x03, 'w',  'w',  'w',  0x06, 'g', 'o',
                            'o',  'g',  'l',  'e',  0x03, 'c', 'o',
                            'm',  0x00, 0x00, 0x02, 0x40, 0x01};

  DnsRecordParser parser(record, sizeof(record), 0);
  base::StringPiece record_strpiece = MakeStringPiece(record, sizeof(record));

  std::unique_ptr<NsecRecordRdata> record_obj =
      NsecRecordRdata::Create(record_strpiece, parser);
  ASSERT_TRUE(record_obj != nullptr);

  ASSERT_EQ(16u, record_obj->bitmap_length());

  EXPECT_FALSE(record_obj->GetBit(0));
  EXPECT_TRUE(record_obj->GetBit(1));
  for (int i = 2; i < 15; i++) {
    EXPECT_FALSE(record_obj->GetBit(i));
  }
  EXPECT_TRUE(record_obj->GetBit(15));

  ASSERT_TRUE(record_obj->IsEqual(record_obj.get()));
}

TEST(RecordRdataTest, CreateNsecRecordWithEmptyBitmapReturnsNull) {
  // These are just the rdata portions of the DNS records, rather than complete
  // records, but it works well enough for this test.
  // This record has a bitmap that is 0 bytes long.
  const uint8_t record[] = {0x03, 'w', 'w',  'w', 0x06, 'g', 'o',  'o',  'g',
                            'l',  'e', 0x03, 'c', 'o',  'm', 0x00, 0x00, 0x00};

  DnsRecordParser parser(record, sizeof(record), 0);
  base::StringPiece record_strpiece = MakeStringPiece(record, sizeof(record));

  std::unique_ptr<NsecRecordRdata> record_obj =
      NsecRecordRdata::Create(record_strpiece, parser);
  ASSERT_FALSE(record_obj);
}

TEST(RecordRdataTest, CreateNsecRecordWithOversizedBitmapReturnsNull) {
  // These are just the rdata portions of the DNS records, rather than complete
  // records, but it works well enough for this test.
  // This record has a bitmap that is 33 bytes long. The maximum size allowed by
  // RFC 3845, Section 2.1.2, is 32 bytes.
  const uint8_t record[] = {
      0x03, 'w',  'w',  'w',  0x06, 'g',  'o',  'o',  'g',  'l',  'e',
      0x03, 'c',  'o',  'm',  0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  DnsRecordParser parser(record, sizeof(record), 0);
  base::StringPiece record_strpiece = MakeStringPiece(record, sizeof(record));

  std::unique_ptr<NsecRecordRdata> record_obj =
      NsecRecordRdata::Create(record_strpiece, parser);
  ASSERT_FALSE(record_obj);
}

TEST(RecordRdataTest, ParseOptRecord) {
  // This is just the rdata portion of an OPT record, rather than a complete
  // record.
  const uint8_t rdata[] = {
      // First OPT
      0x00, 0x01,  // OPT code
      0x00, 0x02,  // OPT data size
      0xDE, 0xAD,  // OPT data
      // Second OPT
      0x00, 0xFF,             // OPT code
      0x00, 0x04,             // OPT data size
      0xDE, 0xAD, 0xBE, 0xEF  // OPT data
  };

  DnsRecordParser parser(rdata, sizeof(rdata), 0);
  base::StringPiece rdata_strpiece = MakeStringPiece(rdata, sizeof(rdata));

  std::unique_ptr<OptRecordRdata> rdata_obj =
      OptRecordRdata::Create(rdata_strpiece, parser);
  ASSERT_THAT(rdata_obj, NotNull());
  ASSERT_THAT(rdata_obj->opts(), SizeIs(2));
  ASSERT_EQ(1, rdata_obj->opts()[0].code());
  ASSERT_EQ("\xde\xad", rdata_obj->opts()[0].data());
  ASSERT_EQ(255, rdata_obj->opts()[1].code());
  ASSERT_EQ("\xde\xad\xbe\xef", rdata_obj->opts()[1].data());
  ASSERT_TRUE(rdata_obj->IsEqual(rdata_obj.get()));
}

TEST(RecordRdataTest, ParseOptRecordWithShorterSizeThanData) {
  // This is just the rdata portion of an OPT record, rather than a complete
  // record.
  const uint8_t rdata[] = {
      0x00, 0xFF,             // OPT code
      0x00, 0x02,             // OPT data size (incorrect, should be 4)
      0xDE, 0xAD, 0xBE, 0xEF  // OPT data
  };

  DnsRecordParser parser(rdata, sizeof(rdata), 0);
  base::StringPiece rdata_strpiece = MakeStringPiece(rdata, sizeof(rdata));

  std::unique_ptr<OptRecordRdata> rdata_obj =
      OptRecordRdata::Create(rdata_strpiece, parser);
  ASSERT_THAT(rdata_obj, IsNull());
}

TEST(RecordRdataTest, ParseOptRecordWithLongerSizeThanData) {
  // This is just the rdata portion of an OPT record, rather than a complete
  // record.
  const uint8_t rdata[] = {
      0x00, 0xFF,  // OPT code
      0x00, 0x04,  // OPT data size (incorrect, should be 4)
      0xDE, 0xAD   // OPT data
  };

  DnsRecordParser parser(rdata, sizeof(rdata), 0);
  base::StringPiece rdata_strpiece = MakeStringPiece(rdata, sizeof(rdata));

  std::unique_ptr<OptRecordRdata> rdata_obj =
      OptRecordRdata::Create(rdata_strpiece, parser);
  ASSERT_THAT(rdata_obj, IsNull());
}

TEST(RecordRdataTest, AddOptToOptRecord) {
  // This is just the rdata portion of an OPT record, rather than a complete
  // record.
  const uint8_t expected_rdata[] = {
      0x00, 0xFF,             // OPT code
      0x00, 0x04,             // OPT data size
      0xDE, 0xAD, 0xBE, 0xEF  // OPT data
  };

  OptRecordRdata rdata;
  rdata.AddOpt(OptRecordRdata::Opt(255, "\xde\xad\xbe\xef"));
  EXPECT_THAT(rdata.buf(), ElementsAreArray(expected_rdata));
}

}  // namespace
}  // namespace net
