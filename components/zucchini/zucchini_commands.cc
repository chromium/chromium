// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/zucchini_commands.h"

#include <stddef.h>
#include <stdint.h>

#include <ostream>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/crc32.h"
#include "components/zucchini/io_utils.h"
#include "components/zucchini/mapped_file.h"
#include "components/zucchini/patch_writer.h"
#include "components/zucchini/zucchini_integration.h"
#include "components/zucchini/zucchini_tools.h"

namespace {

/******** Command-line Switches ********/

constexpr char kSwitchDump[] = "dump";
constexpr char kSwitchImpose[] = "impose";
constexpr char kSwitchKeep[] = "keep";
constexpr char kSwitchRaw[] = "raw";

}  // namespace

zucchini::status::Code MainGen(MainParams params) {
  CHECK_EQ(3U, params.file_paths.size());
  return zucchini::Generate(
      params.file_paths[0], params.file_paths[1], params.file_paths[2],
      params.command_line.HasSwitch(kSwitchKeep),
      params.command_line.HasSwitch(kSwitchRaw),
      params.command_line.GetSwitchValueASCII(kSwitchImpose));
}

zucchini::status::Code MainApply(MainParams params) {
  CHECK_EQ(3U, params.file_paths.size());
  return zucchini::Apply(params.file_paths[0], params.file_paths[1],
                         params.file_paths[2],
                         params.command_line.HasSwitch(kSwitchKeep));
}

zucchini::status::Code MainVerify(MainParams params) {
  CHECK_EQ(1U, params.file_paths.size());
  return zucchini::VerifyPatch(params.file_paths[0]);
}

zucchini::status::Code MainRead(MainParams params) {
  CHECK_EQ(1U, params.file_paths.size());
  base::File input_file(params.file_paths[0],
                        base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_SHARE_DELETE);
  zucchini::MappedFileReader input(std::move(input_file));
  if (input.HasError()) {
    LOG(ERROR) << "Error with file " << params.file_paths[0].value() << ": "
               << input.error();
    return zucchini::status::kStatusFileReadError;
  }

  bool do_dump = params.command_line.HasSwitch(kSwitchDump);
  zucchini::status::Code status = zucchini::ReadReferences(
      {input.data(), input.length()}, do_dump, params.out);
  if (status != zucchini::status::kStatusSuccess)
    params.err << "Fatal error found when dumping references." << std::endl;
  return status;
}

zucchini::status::Code MainDetect(MainParams params) {
  CHECK_EQ(1U, params.file_paths.size());
  base::File input_file(params.file_paths[0],
                        base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_SHARE_DELETE);
  zucchini::MappedFileReader input(std::move(input_file));
  if (input.HasError()) {
    LOG(ERROR) << "Error with file " << params.file_paths[0].value() << ": "
               << input.error();
    return zucchini::status::kStatusFileReadError;
  }

  std::vector<zucchini::ConstBufferView> sub_image_list;
  zucchini::status::Code result = zucchini::DetectAll(
      {input.data(), input.length()}, params.out, &sub_image_list);
  if (result != zucchini::status::kStatusSuccess)
    params.err << "Fatal error found when detecting executables." << std::endl;
  return result;
}

zucchini::status::Code MainMatch(MainParams params) {
  CHECK_EQ(2U, params.file_paths.size());
  using base::File;
  File old_file(params.file_paths[0], File::FLAG_OPEN | File::FLAG_READ |
                                          base::File::FLAG_WIN_SHARE_DELETE);
  zucchini::MappedFileReader old_image(std::move(old_file));
  if (old_image.HasError()) {
    LOG(ERROR) << "Error with file " << params.file_paths[0].value() << ": "
               << old_image.error();
    return zucchini::status::kStatusFileReadError;
  }
  File new_file(params.file_paths[1], File::FLAG_OPEN | File::FLAG_READ |
                                          base::File::FLAG_WIN_SHARE_DELETE);
  zucchini::MappedFileReader new_image(std::move(new_file));
  if (new_image.HasError()) {
    LOG(ERROR) << "Error with file " << params.file_paths[1].value() << ": "
               << new_image.error();
    return zucchini::status::kStatusFileReadError;
  }

  std::string imposed_matches =
      params.command_line.GetSwitchValueASCII(kSwitchImpose);
  zucchini::status::Code status =
      zucchini::MatchAll({old_image.data(), old_image.length()},
                         {new_image.data(), new_image.length()},
                         std::move(imposed_matches), params.out);
  if (status != zucchini::status::kStatusSuccess)
    params.err << "Fatal error found when matching executables." << std::endl;
  return status;
}

zucchini::status::Code MainCrc32(MainParams params) {
  CHECK_EQ(1U, params.file_paths.size());
  base::File image_file(params.file_paths[0],
                        base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_SHARE_DELETE);
  zucchini::MappedFileReader image(std::move(image_file));
  if (image.HasError()) {
    LOG(ERROR) << "Error with file " << params.file_paths[0].value() << ": "
               << image.error();
    return zucchini::status::kStatusFileReadError;
  }

  uint32_t crc =
      zucchini::CalculateCrc32(image.data(), image.data() + image.length());
  params.out << "CRC32: " << zucchini::AsHex<8>(crc) << std::endl;
  return zucchini::status::kStatusSuccess;
}
