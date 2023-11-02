// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/pem.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(PEMTokenizerTest, BasicParsing) {
  const char data[] =
      "-----BEGIN EXPECTED-BLOCK-----\n"
      "TWF0Y2hlc0FjY2VwdGVkQmxvY2tUeXBl\n"
      "-----END EXPECTED-BLOCK-----\n";
  base::StringPiece string_piece(data);
  std::vector<std::string> accepted_types;
  accepted_types.push_back("EXPECTED-BLOCK");

  PEMTokenizer tokenizer(string_piece, accepted_types);
  EXPECT_TRUE(tokenizer.GetNext());

  EXPECT_EQ("EXPECTED-BLOCK", tokenizer.block_type());
  EXPECT_EQ("MatchesAcceptedBlockType", tokenizer.data());

  EXPECT_FALSE(tokenizer.GetNext());
}

TEST(PEMTokenizerTest, CarriageReturnLineFeeds) {
  const char data[] =
      "-----BEGIN EXPECTED-BLOCK-----\r\n"
      "TWF0Y2hlc0FjY2VwdGVkQmxvY2tUeXBl\r\n"
      "-----END EXPECTED-BLOCK-----\r\n";
  base::StringPiece string_piece(data);
  std::vector<std::string> accepted_types;
  accepted_types.push_back("EXPECTED-BLOCK");

  PEMTokenizer tokenizer(string_piece, accepted_types);
  EXPECT_TRUE(tokenizer.GetNext());

  EXPECT_EQ("EXPECTED-BLOCK", tokenizer.block_type());
  EXPECT_EQ("MatchesAcceptedBlockType", tokenizer.data());

  EXPECT_FALSE(tokenizer.GetNext());
}

TEST(PEMTokenizerTest, NoAcceptedBlockTypes) {
  const char data[] =
      "-----BEGIN UNEXPECTED-BLOCK-----\n"
      "SWdub3Jlc1JlamVjdGVkQmxvY2tUeXBl\n"
      "-----END UNEXPECTED-BLOCK-----\n";
  base::StringPiece string_piece(data);
  std::vector<std::string> accepted_types;
  accepted_types.push_back("EXPECTED-BLOCK");

  PEMTokenizer tokenizer(string_piece, accepted_types);
  EXPECT_FALSE(tokenizer.GetNext());
}

TEST(PEMTokenizerTest, MultipleAcceptedBlockTypes) {
  const char data[] =
      "-----BEGIN BLOCK-ONE-----\n"
      "RW5jb2RlZERhdGFPbmU=\n"
      "-----END BLOCK-ONE-----\n"
      "-----BEGIN BLOCK-TWO-----\n"
      "RW5jb2RlZERhdGFUd28=\n"
      "-----END BLOCK-TWO-----\n";
  base::StringPiece string_piece(data);
  std::vector<std::string> accepted_types;
  accepted_types.push_back("BLOCK-ONE");
  accepted_types.push_back("BLOCK-TWO");

  PEMTokenizer tokenizer(string_piece, accepted_types);
  EXPECT_TRUE(tokenizer.GetNext());

  EXPECT_EQ("BLOCK-ONE", tokenizer.block_type());
  EXPECT_EQ("EncodedDataOne", tokenizer.data());

  EXPECT_TRUE(tokenizer.GetNext());

  EXPECT_EQ("BLOCK-TWO", tokenizer.block_type());
  EXPECT_EQ("EncodedDataTwo", tokenizer.data());

  EXPECT_FALSE(tokenizer.GetNext());
}

TEST(PEMTokenizerTest, MissingFooter) {
  const char data[] =
      "-----BEGIN MISSING-FOOTER-----\n"
      "RW5jb2RlZERhdGFPbmU=\n"
      "-----END MISSING-FOOTER-----\n"
      "-----BEGIN MISSING-FOOTER-----\n"
      "RW5jb2RlZERhdGFUd28=\n";
  base::StringPiece string_piece(data);
  std::vector<std::string> accepted_types;
  accepted_types.push_back("MISSING-FOOTER");

  PEMTokenizer tokenizer(string_piece, accepted_types);
  EXPECT_TRUE(tokenizer.GetNext());

  EXPECT_EQ("MISSING-FOOTER", tokenizer.block_type());
  EXPECT_EQ("EncodedDataOne", tokenizer.data());

  EXPECT_FALSE(tokenizer.GetNext());
}

TEST(PEMTokenizerTest, NestedEncoding) {
  const char data[] =
      "-----BEGIN BLOCK-ONE-----\n"
      "RW5jb2RlZERhdGFPbmU=\n"
      "-----BEGIN BLOCK-TWO-----\n"
      "RW5jb2RlZERhdGFUd28=\n"
      "-----END BLOCK-TWO-----\n"
      "-----END BLOCK-ONE-----\n"
      "-----BEGIN BLOCK-ONE-----\n"
      "RW5jb2RlZERhdGFUaHJlZQ==\n"
      "-----END BLOCK-ONE-----\n";
  base::StringPiece string_piece(data);
  std::vector<std::string> accepted_types;
  accepted_types.push_back("BLOCK-ONE");

  PEMTokenizer tokenizer(string_piece, accepted_types);
  EXPECT_TRUE(tokenizer.GetNext());

  EXPECT_EQ("BLOCK-ONE", tokenizer.block_type());
  EXPECT_EQ("EncodedDataThree", tokenizer.data());

  EXPECT_FALSE(tokenizer.GetNext());
}

TEST(PEMTokenizerTest, EmptyAcceptedTypes) {
  const char data[] =
      "-----BEGIN BLOCK-ONE-----\n"
      "RW5jb2RlZERhdGFPbmU=\n"
      "-----END BLOCK-ONE-----\n";
  base::StringPiece string_piece(data);
  std::vector<std::string> accepted_types;

  PEMTokenizer tokenizer(string_piece, accepted_types);
  EXPECT_FALSE(tokenizer.GetNext());
}

TEST(PEMTokenizerTest, BlockWithHeader) {
  const char data[] =
      "-----BEGIN BLOCK-ONE-----\n"
      "Header-One: Data data data\n"
      "Header-Two: \n"
      "  continuation\n"
      "Header-Three: Mix-And,Match\n"
      "\n"
      "RW5jb2RlZERhdGFPbmU=\n"
      "-----END BLOCK-ONE-----\n"
      "-----BEGIN BLOCK-ONE-----\n"
      "RW5jb2RlZERhdGFUd28=\n"
      "-----END BLOCK-ONE-----\n";
  base::StringPiece string_piece(data);
  std::vector<std::string> accepted_types;
  accepted_types.push_back("BLOCK-ONE");

  PEMTokenizer tokenizer(string_piece, accepted_types);
  EXPECT_TRUE(tokenizer.GetNext());

  EXPECT_EQ("BLOCK-ONE", tokenizer.block_type());
  EXPECT_EQ("EncodedDataTwo", tokenizer.data());

  EXPECT_FALSE(tokenizer.GetNext());
}

TEST(PEMEncodeTest, Basic) {
  EXPECT_EQ(
      "-----BEGIN BLOCK-ONE-----\n"
      "RW5jb2RlZERhdGFPbmU=\n"
      "-----END BLOCK-ONE-----\n",
      PEMEncode("EncodedDataOne", "BLOCK-ONE"));
  EXPECT_EQ(
      "-----BEGIN BLOCK-TWO-----\n"
      "RW5jb2RlZERhdGFUd28=\n"
      "-----END BLOCK-TWO-----\n",
      PEMEncode("EncodedDataTwo", "BLOCK-TWO"));
}

TEST(PEMEncodeTest, Empty) {
  EXPECT_EQ(
      "-----BEGIN EMPTY-----\n"
      "-----END EMPTY-----\n",
      PEMEncode("", "EMPTY"));
}

TEST(PEMEncodeTest, Wrapping) {
  EXPECT_EQ(
      "-----BEGIN SINGLE LINE-----\n"
      "MTIzNDU2Nzg5MGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6QUJDREVGR0hJSktM\n"
      "-----END SINGLE LINE-----\n",
      PEMEncode("1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKL",
                "SINGLE LINE"));

  EXPECT_EQ(
      "-----BEGIN WRAPPED LINE-----\n"
      "MTIzNDU2Nzg5MGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6QUJDREVGR0hJSktM\nTQ==\n"
      "-----END WRAPPED LINE-----\n",
      PEMEncode("1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLM",
                "WRAPPED LINE"));
}

}  // namespace net
