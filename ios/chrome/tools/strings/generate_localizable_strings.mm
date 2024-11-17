// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper tool that is built and run during a build to pull strings from the
// GRD files and generate a localized string files needed for iOS app bundles.
// Arguments:
//   -p dir_to_data_pak
//   -o output_dir
//   -c config_file
//   -I header_root_dir
//   and a list of locales.
//
// Example:
// generate_localizable_strings \
//   -p "${SHARED_INTERMEDIATE_DIR}/repack_ios/repack" \
//   -o "${INTERMEDIATE_DIR}/app_infoplist_strings" \
//   -c "<(DEPTH)/ios/chrome/localizable_strings_config.plist" \
//   -I "${SHARED_INTERMEDIATE_DIR}" \
//   ar ca cs da de el en-GB en-US es fi fr he hr hu id it ja ko ms nb nl pl \
//   pt pt-PT ro ru sk sv th tr uk vi zh-CN zh-TW

#import <Foundation/Foundation.h>
#import <stdio.h>

#import <map>
#import <set>
#import <string>
#import <string_view>
#import <utility>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/tools/strings/grit_header_parsing.h"
#import "ui/base/resource/data_pack.h"
#import "ui/base/resource/resource_handle.h"
#import "ui/base/resource/resource_scale_factor.h"

namespace {

// Load the packed resource data pack for |locale| from |packed_data_pack_dir|.
// If loading fails, null is returned.
std::unique_ptr<ui::DataPack> LoadResourceDataPack(
    NSString* packed_data_pack_dir,
    NSString* locale_name) {
  std::unique_ptr<ui::DataPack> resource_data_pack;
  NSString* resource_path =
      [NSString stringWithFormat:@"%@/%@.lproj/locale.pak",
                                 packed_data_pack_dir, locale_name];

  if (!resource_path)
    return resource_data_pack;

  // FilePath may contain components that references parent directory
  // (".."). DataPack disallows paths with ".." for security reasons.
  base::FilePath resources_pak_path([resource_path fileSystemRepresentation]);
  resources_pak_path = base::MakeAbsoluteFilePath(resources_pak_path);
  if (!base::PathExists(resources_pak_path))
    return resource_data_pack;

  resource_data_pack.reset(new ui::DataPack(ui::k100Percent));
  if (!resource_data_pack->LoadFromPath(resources_pak_path))
    resource_data_pack.reset();

  return resource_data_pack;
}

// Create a |NSString| from the string with |resource_id| from |data_pack|.
// Return nil if none is found.
NSString* GetStringFromDataPack(const ui::DataPack& data_pack,
                                uint16_t resource_id) {
  std::optional<std::string_view> data = data_pack.GetStringView(resource_id);
  if (!data.has_value()) {
    return nil;
  }

  // Data pack encodes strings as either UTF8 or UTF16.
  if (data_pack.GetTextEncodingType() == ui::DataPack::UTF8) {
    return [[NSString alloc] initWithBytes:data->data()
                                    length:data->length()
                                  encoding:NSUTF8StringEncoding];
  } else if (data_pack.GetTextEncodingType() == ui::DataPack::UTF16) {
    return [[NSString alloc] initWithBytes:data->data()
                                    length:data->length()
                                  encoding:NSUTF16LittleEndianStringEncoding];
  }
  return nil;
}

// Generates a NSDictionary mapping string IDs to localized strings. The
// dictionary can be written as a Property List (only contains types that
// are valid in Propery Lists).
NSDictionary* GenerateLocalizableStringsDictionary(
    const ui::DataPack& data_pack,
    const char* locale,
    NSArray* resources,
    NSDictionary* resources_ids) {
  NSMutableDictionary* dictionary = [NSMutableDictionary dictionary];
  for (id resource : resources) {
    NSString* resource_name = nil;
    NSString* resource_output_name = nil;
    if ([resource isKindOfClass:[NSString class]]) {
      resource_name = resource;
      resource_output_name = resource;
    } else if ([resource isKindOfClass:[NSDictionary class]]) {
      resource_name = [resource objectForKey:@"input"];
      resource_output_name = [resource objectForKey:@"output"];
      if (!resource_name || !resource_output_name) {
        fprintf(
            stderr,
            "ERROR: resources must be given in <string> or <dict> format.\n");
        return nil;
      }
    } else {
      fprintf(stderr,
              "ERROR: resources must be given in <string> or <dict> format.\n");
      return nil;
    }
    NSInteger resource_id =
        [[resources_ids objectForKey:resource_name] integerValue];
    NSString* string = GetStringFromDataPack(data_pack, resource_id);
    if (string) {
      [dictionary setObject:string forKey:resource_output_name];
    } else {
      fprintf(stderr, "ERROR: fail to load string '%s' for locale '%s'\n",
              base::SysNSStringToUTF8(resource_name).c_str(), locale);
      return nil;
    }
  }

  return dictionary;
}

NSDictionary* LoadResourcesListFromHeaders(NSArray* header_list,
                                           NSString* root_header_dir) {
  if (![header_list count]) {
    fprintf(stderr, "ERROR: No header file in the config.\n");
    return nil;
  }

  std::vector<base::FilePath> headers;
  for (NSString* header in header_list) {
    headers.push_back(base::apple::NSStringToFilePath(
        [root_header_dir stringByAppendingPathComponent:header]));
  }

  std::optional<ResourceMap> resource_map =
      LoadResourcesFromGritHeaders(headers);
  if (!resource_map) {
    return nil;
  }

  NSMutableDictionary* resource_ids = [[NSMutableDictionary alloc] init];
  for (const auto& pair : *resource_map) {
    resource_ids[base::SysUTF8ToNSString(pair.first)] =
        [NSNumber numberWithInt:pair.second];
  }

  return [resource_ids copy];
}

// Save |dictionary| as a Property List file (in binary1 encoding)
// with |locale| to |output_dir|/|locale|.lproj/|output_filename|.
bool SavePropertyList(NSDictionary* dictionary,
                      NSString* locale,
                      NSString* output_dir,
                      NSString* output_filename) {
  // Compute the path to the output directory with locale.
  NSString* output_path = [output_dir
      stringByAppendingPathComponent:[NSString
                                         stringWithFormat:@"%@.lproj", locale]];

  // Prepare the directory.
  NSFileManager* file_manager = [NSFileManager defaultManager];
  if (![file_manager fileExistsAtPath:output_path] &&
      ![file_manager createDirectoryAtPath:output_path
               withIntermediateDirectories:YES
                                attributes:nil
                                     error:nil]) {
    fprintf(stderr, "ERROR: '%s' didn't exist or failed to create it\n",
            base::SysNSStringToUTF8(output_path).c_str());
    return false;
  }

  // Convert to property list in binary format.
  NSError* error = nil;
  NSData* data = [NSPropertyListSerialization
      dataWithPropertyList:dictionary
                    format:NSPropertyListBinaryFormat_v1_0
                   options:0
                     error:&error];
  if (!data) {
    fprintf(stderr, "ERROR: conversion to property list failed: %s\n",
            base::SysNSStringToUTF8([error localizedDescription]).c_str());
    return false;
  }

  // Save the strings to the disk.
  output_path = [output_path stringByAppendingPathComponent:output_filename];
  if (![data writeToFile:output_path atomically:YES]) {
    fprintf(stderr, "ERROR: Failed to write out '%s'\n",
            base::SysNSStringToUTF8(output_filename).c_str());
    return false;
  }

  return true;
}

int real_main(int argc, char* const argv[]) {
  NSString* output_dir = nil;
  NSString* data_pack_dir = nil;
  NSString* root_header_dir = nil;
  NSString* config_file = nil;

  int ch;
  while ((ch = getopt(argc, argv, "c:I:p:o:h")) != -1) {
    switch (ch) {
      case 'c':
        config_file = base::SysUTF8ToNSString(optarg);
        break;
      case 'I':
        root_header_dir = base::SysUTF8ToNSString(optarg);
        break;
      case 'p':
        data_pack_dir = base::SysUTF8ToNSString(optarg);
        break;
      case 'o':
        output_dir = base::SysUTF8ToNSString(optarg);
        break;
      case 'h':
        fprintf(stdout,
                "usage: generate_localizable_strings  -p data_pack_dir "
                "-o output_dir -c config_file -I input_root "
                "locale [locale...]\n"
                "\n"
                "Generate localized string files specified in |config_file|\n"
                "for all specified locales in output_dir from packed data\n"
                "packs in data_pack_dir.\n");
        exit(0);
      default:
        fprintf(stderr, "ERROR: bad command line arg: %c.n\n", ch);
        exit(1);
    }
  }

  if (!config_file) {
    fprintf(stderr, "ERROR: missing config file.\n");
    exit(1);
  }

  if (!root_header_dir) {
    fprintf(stderr, "ERROR: missing root header dir.\n");
    exit(1);
  }

  if (!output_dir) {
    fprintf(stderr, "ERROR: missing output directory.\n");
    exit(1);
  }

  if (!data_pack_dir) {
    fprintf(stderr, "ERROR: missing data pack directory.\n");
    exit(1);
  }

  if (optind == argc) {
    fprintf(stderr, "ERROR: missing locale list.\n");
    exit(1);
  }

  NSDictionary* config =
      [NSDictionary dictionaryWithContentsOfFile:config_file];

  NSDictionary* resources_ids = LoadResourcesListFromHeaders(
      [config objectForKey:@"headers"], root_header_dir);

  if (!resources_ids) {
    exit(1);
  }

  NSMutableArray* locales = [NSMutableArray arrayWithCapacity:(argc - optind)];
  for (int i = optind; i < argc; ++i) {
    // In order to find the locale at runtime, it needs to use '_' instead of
    // '-' (http://crbug.com/20441).  Also, 'en-US' should be represented
    // simply as 'en' (http://crbug.com/19165, http://crbug.com/25578).
    NSString* locale = [NSString stringWithUTF8String:argv[i]];
    if ([locale isEqualToString:@"en-US"]) {
      locale = @"en";
    } else {
      locale = [locale stringByReplacingOccurrencesOfString:@"-"
                                                 withString:@"_"];
    }
    [locales addObject:locale];
  }

  NSArray* outputs = [config objectForKey:@"outputs"];

  if (![outputs count]) {
    fprintf(stderr, "ERROR: No output in config file\n");
    exit(1);
  }

  for (NSString* locale in locales) {
    std::unique_ptr<ui::DataPack> data_pack =
        LoadResourceDataPack(data_pack_dir, locale);
    if (!data_pack) {
      fprintf(stderr, "ERROR: Failed to load branded pak for language: %s\n",
              base::SysNSStringToUTF8(locale).c_str());
      exit(1);
    }

    for (NSDictionary* output : [config objectForKey:@"outputs"]) {
      NSString* output_name = [output objectForKey:@"name"];
      if (!output_name) {
        fprintf(stderr, "ERROR: Output without name.\n");
        exit(1);
      }
      NSArray* output_strings = [output objectForKey:@"strings"];
      if (![output_strings count]) {
        fprintf(stderr, "ERROR: Output without strings: %s.\n",
                base::SysNSStringToUTF8(output_name).c_str());
        exit(1);
      }

      NSDictionary* dictionary = GenerateLocalizableStringsDictionary(
          *data_pack, base::SysNSStringToUTF8(locale).c_str(), output_strings,
          resources_ids);
      if (dictionary) {
        SavePropertyList(dictionary, locale, output_dir, output_name);
      } else {
        fprintf(stderr, "ERROR: Unable to create %s.\n",
                base::SysNSStringToUTF8(output_name).c_str());
        exit(1);
      }
    }
  }
  return 0;
}

}  // namespace

int main(int argc, char* const argv[]) {
  @autoreleasepool {
    return real_main(argc, argv);
  }
}
