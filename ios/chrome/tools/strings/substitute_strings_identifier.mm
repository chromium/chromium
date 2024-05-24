// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <stdio.h>
#import <unistd.h>

#import <string>

#import "base/apple/foundation_util.h"
#import "base/files/file.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/tools/strings/grit_header_parsing.h"

namespace {

using PList = NSDictionary<NSString*, NSObject*>;

const char kUsageString[] = R"(
usage: substitute_strings_identifier -I header_path -i source_path -i output_path

Loads the plist at `source_path` and replace all string values corresponding
to string identifiers to their numerical values (as found in `header_path`)
and write the resulting plist to `output_path`.
)";

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
      fprintf(stderr, "ERROR: no value found for string: %s\n", key.c_str());
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
    fprintf(stderr, "ERROR: loading %s failed: %s\n",
            source_path.AsUTF8Unsafe().c_str(),
            base::SysNSStringToUTF8(error.localizedDescription).c_str());
    return false;
  }

  PList* output_plist = ConvertPlist(source_plist, resource_map);
  if (!output_plist) {
    return false;
  }

  base::File::Error file_error;
  const base::FilePath output_dir = output_path.DirName();
  if (!base::CreateDirectoryAndGetError(output_dir, &file_error)) {
    fprintf(stderr, "ERROR: creating %s failed: %s\n",
            output_dir.AsUTF8Unsafe().c_str(),
            base::File::ErrorToString(file_error).c_str());
    return false;
  }

  if (![output_plist writeToURL:output_url error:&error]) {
    fprintf(stderr, "ERROR: writing %s failed: %s\n",
            output_path.AsUTF8Unsafe().c_str(),
            base::SysNSStringToUTF8(error.localizedDescription).c_str());
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
          fprintf(stderr, "ERROR: cannot pass -i multiple times\n");
          return 1;
        }

        source_path = base::FilePath(optarg);
        break;

      case 'o':
        if (!output_path.empty()) {
          fprintf(stderr, "ERROR: cannot pass -o multiple times\n");
          return 1;
        }

        output_path = base::FilePath(optarg);
        break;

      case 'h':
        fprintf(stdout, "%s", kUsageString + 1);
        return 0;

      default:
        fprintf(stderr, "ERROR: unknown argument: -%c\n", ch);
        return 1;
    }
  }

  if (headers.empty()) {
    fprintf(stderr, "ERROR: header_path is required.\n");
    return 1;
  }

  if (source_path.empty()) {
    fprintf(stderr, "ERROR: source_path is required.\n");
    return 1;
  }

  if (output_path.empty()) {
    fprintf(stderr, "ERROR: output_path is required.\n");
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
