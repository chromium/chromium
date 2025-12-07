// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <unistd.h>

#import <iostream>
#import <string>
#import <string_view>

#import "base/apple/foundation_util.h"
#import "base/files/file.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/tools/strings/grit_header_parsing.h"

namespace {

using PList = NSDictionary<NSString*, NSObject*>;

constexpr char kUsageString[] =
    R"(usage: substitute_strings_identifier -I header_path -i source_path -i output_path

Loads the plist at `source_path` and replace all string values corresponding
to string identifiers to their numerical values (as found in `header_path`)
and write the resulting plist to `output_path`.)";

NSObject* ConvertValue(NSObject* value, const ResourceMap& resource_map);

PList* ConvertPlist(PList* plist, const ResourceMap& resource_map) {
  NSMutableDictionary* converted = [[NSMutableDictionary alloc] init];
  for (NSString* key in plist) {
    NSObject* object = ConvertValue(plist[key], resource_map);
    if (!object) {
      return nil;
    }

    converted[key] = object;
  }
  return [converted copy];
}

NSObject* ConvertString(NSString* string, const ResourceMap& resource_map) {
  const std::string key = base::SysNSStringToUTF8(string);
  auto iter = resource_map.find(key);
  if (iter == resource_map.end()) {
    if (base::StartsWith(key, "IDS_") || base::StartsWith(key, "IDR_")) {
      std::cerr << "ERROR: no value found for string: " << key << std::endl;
      return nil;
    }

    return string;
  }

  return [NSNumber numberWithInt:iter->second];
}

NSArray* ConvertArray(NSArray* array, const ResourceMap& resource_map) {
  NSMutableArray* converted = [[NSMutableArray alloc] init];
  for (NSObject* value in array) {
    NSObject* object = ConvertValue(value, resource_map);
    if (!object) {
      return nil;
    }

    [converted addObject:object];
  }
  return [converted copy];
}

NSObject* ConvertValue(NSObject* value, const ResourceMap& resource_map) {
  if ([value isKindOfClass:[NSString class]]) {
    NSString* string = base::apple::ObjCCastStrict<NSString>(value);
    return ConvertString(string, resource_map);
  }

  if ([value isKindOfClass:[NSArray class]]) {
    NSArray<NSObject*>* array = base::apple::ObjCCastStrict<NSArray>(value);
    return ConvertArray(array, resource_map);
  }

  if ([value isKindOfClass:[NSDictionary class]]) {
    PList* plist = base::apple::ObjCCastStrict<NSDictionary>(value);
    return ConvertPlist(plist, resource_map);
  }

  return value;
}

bool ConvertFile(const base::FilePath& source_path,
                 const base::FilePath& output_path,
                 const ResourceMap& resource_map) {
  NSURL* source_url = base::apple::FilePathToNSURL(source_path);
  NSURL* output_url = base::apple::FilePathToNSURL(output_path);

  NSError* error = nil;
  PList* source_plist = [NSDictionary dictionaryWithContentsOfURL:source_url
                                                            error:&error];
  if (error) {
    std::cerr << "ERROR: loading '" << source_path << "'  failed: "
              << base::SysNSStringToUTF8(error.localizedDescription)
              << std::endl;
    return false;
  }

  PList* output_plist = ConvertPlist(source_plist, resource_map);
  if (!output_plist) {
    return false;
  }

  base::File::Error file_error;
  const base::FilePath output_dir = output_path.DirName();
  if (!base::CreateDirectoryAndGetError(output_dir, &file_error)) {
    std::cerr << "ERROR: creating '" << output_dir
              << "' failed: " << base::File::ErrorToString(file_error)
              << std::endl;
    return false;
  }

  if (![output_plist writeToURL:output_url error:&error]) {
    std::cerr << "ERROR: writing '" << output_path << "' failed: "
              << base::SysNSStringToUTF8(error.localizedDescription)
              << std::endl;
    return false;
  }

  return true;
}

int RealMain(int argc, char* const argv[]) {
  base::FilePath source_path;
  base::FilePath output_path;
  std::vector<base::FilePath> headers;

  int ch = 0;
  while ((ch = getopt(argc, argv, "I:i:o:h")) != -1) {
    switch (ch) {
      case 'I':
        headers.push_back(base::FilePath(optarg));
        break;

      case 'i':
        if (!source_path.empty()) {
          std::cerr << "ERROR: cannot pass -i multiple times" << std::endl;
          return 1;
        }

        source_path = base::FilePath(optarg);
        break;

      case 'o':
        if (!output_path.empty()) {
          std::cerr << "ERROR: cannot pass -o multiple times" << std::endl;
          return 1;
        }

        output_path = base::FilePath(optarg);
        break;

      case 'h':
        std::cerr << kUsageString << std::endl;
        return 0;

      default:
        std::cerr << "ERROR: unknown argument: -" << ch << std::endl;
        return 1;
    }
  }

  if (headers.empty()) {
    std::cerr << "ERROR: header_path is required." << std::endl;
    return 1;
  }

  if (source_path.empty()) {
    std::cerr << "ERROR: source_path is required." << std::endl;
    return 1;
  }

  if (output_path.empty()) {
    std::cerr << "ERROR: output_path is required." << std::endl;
    return 1;
  }

  std::optional<ResourceMap> resource_map =
      LoadResourcesFromGritHeaders(headers);

  if (!resource_map) {
    return 1;
  }

  if (!ConvertFile(source_path, output_path, *resource_map)) {
    return 1;
  }

  return 0;
}

}  // anonymous namespace

int main(int argc, char* const argv[]) {
  @autoreleasepool {
    return RealMain(argc, argv);
  }
}
