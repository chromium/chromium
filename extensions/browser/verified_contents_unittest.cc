// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <vector>

#include "base/base64url.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "extensions/browser/content_verifier/content_verifier_utils.h"
#include "extensions/browser/verified_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char kContentVerifierDirectory[] = "content_verifier/";
const char kPublicKeyPem[] = "public_key.pem";

constexpr bool kIsFileAccessCaseInsensitive =
    !content_verifier_utils::IsFileAccessCaseSensitive();
constexpr bool kIsDotSpaceSuffixIgnored =
    content_verifier_utils::IsDotSpaceFilenameSuffixIgnored();

std::string DecodeBase64Url(const std::string& encoded) {
  std::string decoded;
  if (!base::Base64UrlDecode(
          encoded, base::Base64UrlDecodePolicy::IGNORE_PADDING, &decoded))
    return std::string();

  return decoded;
}

bool GetPublicKey(const base::FilePath& path, std::string* public_key) {
  std::string public_key_pem;
  if (!base::ReadFileToString(path, &public_key_pem)) {
    return false;
  }
  if (!Extension::ParsePEMKeyBytes(public_key_pem, public_key)) {
    return false;
  }
  return true;
}

base::FilePath GetTestDir(const char* sub_dir) {
  base::FilePath path;
  base::PathService::Get(DIR_TEST_DATA, &path);
  return path.AppendASCII(kContentVerifierDirectory).AppendASCII(sub_dir);
}

// Loads verified_contents file from a sub directory under
// kContentVerifierDirectory.
std::unique_ptr<VerifiedContents> CreateTestVerifiedContents(
    const char* sub_dir,
    const char* verified_contents_filename) {
  // Figure out our test data directory.
  base::FilePath path = GetTestDir(sub_dir);

  // Initialize the VerifiedContents object.
  std::string public_key;
  if (!GetPublicKey(path.AppendASCII(kPublicKeyPem), &public_key)) {
    return nullptr;
  }

  base::FilePath verified_contents_path =
      path.AppendASCII(verified_contents_filename);
  return VerifiedContents::CreateFromFile(
      base::as_bytes(base::make_span(public_key)), verified_contents_path);
}

}  // namespace

TEST(VerifiedContents, Simple) {
  std::unique_ptr<VerifiedContents> verified_contents =
      CreateTestVerifiedContents("simple", "verified_contents.json");
  ASSERT_TRUE(verified_contents);
  const VerifiedContents& contents = *verified_contents;

  // Make sure we get expected values.
  EXPECT_EQ(contents.block_size(), 4096);
  EXPECT_EQ(contents.extension_id(), "abcdefghijklmnopabcdefghijklmnop");
  EXPECT_EQ("1.2.3", contents.version().GetString());

  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("manifest.json"),
      DecodeBase64Url("-vyyIIn7iSCzg7X3ICUI5wZa3tG7w7vyiCckxZdJGfs")));

  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("background.js"),
      DecodeBase64Url("txHiG5KQvNoPOSH5FbQo9Zb5gJ23j3oFB0Ru9DOnziw")));

  base::FilePath foo_bar_html =
      base::FilePath(FILE_PATH_LITERAL("foo")).AppendASCII("bar.html");
  EXPECT_FALSE(foo_bar_html.IsAbsolute());
  EXPECT_TRUE(contents.TreeHashRootEquals(
      foo_bar_html,
      DecodeBase64Url("L37LFbT_hmtxRL7AfGZN9YTpW6yoz_ZiQ1opLJn1NZU")));

  base::FilePath nonexistent = base::FilePath::FromUTF8Unsafe("nonexistent");
  EXPECT_FALSE(contents.HasTreeHashRoot(nonexistent));

  std::map<std::string, std::string> hashes = {
      {"lowercase.html", "HpLotLGCmmOdKYvGQmD3OkXMKGs458dbanY4WcfAZI0"},
      {"ALLCAPS.html", "bl-eV8ENowvtw6P14D4X1EP0mlcMoG-_aOx5o9C1364"},
      {"MixedCase.Html", "zEAO9FwciigMNy3NtU2XNb-dS5TQMmVNx0T9h7WvXbQ"},
      {"mIxedcAse.Html", "nKRqUcJg1_QZWAeCb4uFd5ouC0McuGavKp8TFDRqBgg"},
  };

  // Resource is "lowercase.html".
  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("lowercase.html"),
      DecodeBase64Url(hashes["lowercase.html"])));
  // Only case-insensitive systems should be able to get hashes with incorrect
  // case.
  EXPECT_EQ(kIsFileAccessCaseInsensitive,
            contents.TreeHashRootEquals(
                base::FilePath::FromUTF8Unsafe("Lowercase.Html"),
                DecodeBase64Url(hashes["lowercase.html"])));
  EXPECT_EQ(kIsFileAccessCaseInsensitive,
            contents.TreeHashRootEquals(
                base::FilePath::FromUTF8Unsafe("LOWERCASE.HTML"),
                DecodeBase64Url(hashes["lowercase.html"])));

  // Resource is "ALLCAPS.HTML"
  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("ALLCAPS.HTML"),
      DecodeBase64Url(hashes["ALLCAPS.html"])));
  // Only case-insensitive systems should be able to get hashes with incorrect
  // case.
  EXPECT_EQ(kIsFileAccessCaseInsensitive,
            contents.TreeHashRootEquals(
                base::FilePath::FromUTF8Unsafe("AllCaps.Html"),
                DecodeBase64Url(hashes["ALLCAPS.html"])));
  EXPECT_EQ(kIsFileAccessCaseInsensitive,
            contents.TreeHashRootEquals(
                base::FilePath::FromUTF8Unsafe("allcaps.html"),
                DecodeBase64Url(hashes["ALLCAPS.html"])));

  // Resources are "MixedCase.Html", "mIxedcAse.Html".
  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("MixedCase.Html"),
      DecodeBase64Url(hashes["MixedCase.Html"])));
  EXPECT_TRUE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("mIxedcAse.Html"),
      DecodeBase64Url(hashes["mIxedcAse.Html"])));
  // In case-sensitive systems, swapping hashes within MixedCase.Html and
  // mIxedcAse.Html always would mismatch hash, but it matches for
  // case-insensitive systems.
  // TODO(https:://crbug.com/1040702): Fix if this becomes a problem.
  EXPECT_EQ(kIsFileAccessCaseInsensitive,
            contents.TreeHashRootEquals(
                base::FilePath::FromUTF8Unsafe("mIxedcAse.Html"),
                DecodeBase64Url(hashes["MixedCase.Html"])));
  EXPECT_EQ(kIsFileAccessCaseInsensitive,
            contents.TreeHashRootEquals(
                base::FilePath::FromUTF8Unsafe("MixedCase.Html"),
                DecodeBase64Url(hashes["mIxedcAse.Html"])));
  // Continuing from above, in case-insensitive systems, there is non
  // deterministic behavior here, e.g. MIXEDCASE.HTML will match both hashes of
  // MixedCase.Html and mIxedcAse.Html.
  EXPECT_EQ(kIsFileAccessCaseInsensitive,
            contents.TreeHashRootEquals(
                base::FilePath::FromUTF8Unsafe("MIXEDCASE.HTML"),
                DecodeBase64Url(hashes["MixedCase.Html"])));
  EXPECT_EQ(kIsFileAccessCaseInsensitive,
            contents.TreeHashRootEquals(
                base::FilePath::FromUTF8Unsafe("MIXEDCASE.HTML"),
                DecodeBase64Url(hashes["mIxedcAse.Html"])));
  EXPECT_EQ(kIsFileAccessCaseInsensitive,
            contents.TreeHashRootEquals(
                base::FilePath::FromUTF8Unsafe("mixedcase.html"),
                DecodeBase64Url(hashes["MixedCase.Html"])));
  EXPECT_EQ(kIsFileAccessCaseInsensitive,
            contents.TreeHashRootEquals(
                base::FilePath::FromUTF8Unsafe("mixedcase.html"),
                DecodeBase64Url(hashes["mIxedcAse.Html"])));

  // Regression test for https://crbug.com/776609.
  EXPECT_FALSE(contents.TreeHashRootEquals(
      base::FilePath::FromUTF8Unsafe("allcaps.html"),
      // This is the hash of "mixedcase.html".
      DecodeBase64Url("zEAO9FwciigMNy3NtU2XNb-dS5TQMmVNx0T9h7WvXbQ")));
}

TEST(VerifiedContents, FailsOnBase64) {
  // Accepting base64-encoded input where base64url-encoded input is expected
  // will be considered to be invalid data. Verify that it gets rejected.
  ASSERT_FALSE(
      CreateTestVerifiedContents("simple", "verified_contents_base64.json"));
}

// Tests behavior of verified contents with filenames that have "." and " "
// suffixes appened to them.
// Regression test for https://crbug.com/696208.
TEST(VerifiedContents, DotSpaceSuffixedFiles) {
  std::unique_ptr<VerifiedContents> contents =
      CreateTestVerifiedContents("dot_space_suffix", "verified_contents.json");
  ASSERT_TRUE(contents);

  // Make sure we get expected values.
  EXPECT_EQ(contents->block_size(), 4096);
  EXPECT_EQ(contents->extension_id(), "abcdabcdabcdabcdabcdabcdabcdabcd");
  EXPECT_EQ("1.2.3.4", contents->version().GetString());

  auto has_tree_hash_root = [&contents](const std::string& file_path_str) {
    return contents->HasTreeHashRoot(
        base::FilePath::FromUTF8Unsafe(file_path_str));
  };
  auto tree_hash_root_equals = [&contents](const std::string& file_path_str,
                                           const char* expected_hash) {
    return contents->TreeHashRootEquals(
        base::FilePath::FromUTF8Unsafe(file_path_str),
        DecodeBase64Url(expected_hash));
  };

  // Non-existent files won't be found in tree hash.
  EXPECT_FALSE(has_tree_hash_root("non-existent.js"));
  EXPECT_FALSE(has_tree_hash_root(""));

  struct TestFileInfo {
    const char* filename;
    const char* root_hashes;
  };
  std::vector<TestFileInfo> info_list{
      {
          "manifest.json", "ysCDJuQ1s7vWF4yUZTRB2_XDE6vfFyQcIPSmyvNvqEw",
      },
      {
          "background.js", "uYeF7eHzVgKpiBg5fikv2NTctmJnxCfX1UhhlrizvNE",
      },
      {
          "mixedcase.html", "S1lnRa4Yu1CM2dCwJoFYKfAqRkFC7SSI4tzyIOzO7hA",
      },
      {
          "mixedCase.html", "FVncNmt1wBfFn3aZVTnMB9CFRTRIl0Z4YFqm14Wmrhs",
      },
      {
          "doT.html.", "jEsJEk-0azFYx7G91rSUPuzPBXp95863lG4MDwZcSog",
      },
  };

  std::vector<std::string> kSuffixes{
      // Only spaces.
      " ", "  ", "   ",
      // Only dots.
      ".", "..", "...",
      // Mix of dots and spaces.
      ". ", ".  ", ".. ", "... ", " .", "  .", "   .", " . ", " ..", " ...",
      " .. ",
  };

  for (const TestFileInfo& info : info_list) {
    // The original filenames' hashes must exist.
    EXPECT_TRUE(has_tree_hash_root(info.filename));
    EXPECT_TRUE(tree_hash_root_equals(info.filename, info.root_hashes));

    // Verify that the discovery of tree hashes is also correct when the
    // filenames are appended with dot and space characters:
    //   - they should still succeed on windows (kIsDotSpaceSuffixIgnored
    //     = true).
    //   - they should fail otherwise (kIsDotSpaceSuffixIgnored = false).
    for (const std::string& suffix : kSuffixes) {
      std::string path_with_suffix = std::string(info.filename).append(suffix);
      EXPECT_EQ(kIsDotSpaceSuffixIgnored, has_tree_hash_root(path_with_suffix));
    }
  }

  // For background.js, additionally verify that reading the file with and
  // without the suffixes described above matches our expectations, taking
  // kIsDotSpaceSuffixIgnored into account.
  const char* kBackgroundJSFilename = "background.js";
  const char* kBackgroundJSContents = "console.log('hello');\n";
  base::FilePath test_dir = GetTestDir("dot_space_suffix");
  {
    // Case 1/2: background.js without suffix.
    base::FilePath background_js_path =
        test_dir.AppendASCII(kBackgroundJSFilename);
    EXPECT_TRUE(base::PathExists(background_js_path));
    std::string background_js_contents;
    EXPECT_TRUE(
        base::ReadFileToString(background_js_path, &background_js_contents));
    EXPECT_EQ(kBackgroundJSContents, background_js_contents);
  }
  {
    // Case 2/2: background.js with dot/space suffixes.
    for (const std::string& suffix : kSuffixes) {
      base::FilePath background_js_suffix_path = test_dir.AppendASCII(
          std::string(kBackgroundJSFilename).append(suffix));
      EXPECT_EQ(kIsDotSpaceSuffixIgnored,
                base::PathExists(background_js_suffix_path));
      if (kIsDotSpaceSuffixIgnored) {
        std::string background_js_suffix_contents;
        EXPECT_TRUE(base::ReadFileToString(background_js_suffix_path,
                                           &background_js_suffix_contents));
        EXPECT_EQ(kBackgroundJSContents, background_js_suffix_contents);
      }
    }
  }
}

// Tests behavior of verified_contents.json file containing keys already with
// "." suffix.
// Regression test for https://crbug.com/696208.
TEST(VerifiedContents, VerifiedContentsFileContainsDotSuffixedFilename) {
  std::unique_ptr<VerifiedContents> contents =
      CreateTestVerifiedContents("dot_space_suffix", "verified_contents.json");
  ASSERT_TRUE(contents);

  // Make sure we get expected values.
  EXPECT_EQ(contents->block_size(), 4096);
  EXPECT_EQ(contents->extension_id(), "abcdabcdabcdabcdabcdabcdabcdabcd");
  EXPECT_EQ("1.2.3.4", contents->version().GetString());

  auto has_tree_hash_root = [&contents](const std::string& file_path_str) {
    return contents->HasTreeHashRoot(
        base::FilePath::FromUTF8Unsafe(file_path_str));
  };
  auto tree_hash_root_equals = [&contents](const std::string& file_path_str,
                                           const char* expected_hash) {
    return contents->TreeHashRootEquals(
        base::FilePath::FromUTF8Unsafe(file_path_str),
        DecodeBase64Url(expected_hash));
  };

  // The original key "doT.html." should always succeed.
  EXPECT_TRUE(has_tree_hash_root("doT.html."));
  EXPECT_TRUE(tree_hash_root_equals(
      "doT.html.", "jEsJEk-0azFYx7G91rSUPuzPBXp95863lG4MDwZcSog"));
  // Its case variants would only succeed for case-insensitive system.
  EXPECT_EQ(kIsFileAccessCaseInsensitive, has_tree_hash_root("dot.html."));
  EXPECT_EQ(kIsFileAccessCaseInsensitive,
            tree_hash_root_equals(
                "dot.html.", "jEsJEk-0azFYx7G91rSUPuzPBXp95863lG4MDwZcSog"));

  // Keys with dot stripped succeeds if kIsDotSpaceSuffixIgnored is true.
  {
    const char* kKey = "dot.html";
    EXPECT_EQ(kIsDotSpaceSuffixIgnored, has_tree_hash_root(kKey));
    EXPECT_EQ(kIsDotSpaceSuffixIgnored,
              tree_hash_root_equals(
                  kKey, "jEsJEk-0azFYx7G91rSUPuzPBXp95863lG4MDwZcSog"));
  }

  // Also, adding (.| )+ suffix would succeed if kIsDotSpaceSuffixIgnored is
  // true. This is already part of VerifiedContents.DotSpaceSuffixedFiles test.
}

}  // namespace extensions
