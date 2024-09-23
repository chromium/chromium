// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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
#include "components/zucchini/encoded_view.h"
#include "components/zucchini/image_index.h"
#include "components/zucchini/io_utils.h"
#include "components/zucchini/mapped_file.h"
#include "components/zucchini/patch_writer.h"
#include "components/zucchini/suffix_array.h"
#include "components/zucchini/zucchini_integration.h"
#include "components/zucchini/zucchini_tools.h"

namespace {

using zucchini::status::kStatusFileReadError;
using zucchini::status::kStatusSuccess;

/******** Command-line Switches ********/

constexpr char kSwitchDump[] = "dump";
constexpr char kSwitchImpose[] = "impose";
constexpr char kSwitchKeep[] = "keep";
constexpr char kSwitchRaw[] = "raw";

class WrappedMappedFileReader : public zucchini::MappedFileReader {
 public:
  static constexpr uint32_t read_flags = base::File::FLAG_OPEN |
                                         base::File::FLAG_READ |
                                         base::File::FLAG_WIN_SHARE_DELETE;

  explicit WrappedMappedFileReader(const base::FilePath& path)
      : MappedFileReader(base::File(path, read_flags)) {
    if (HasError()) {
      LOG(ERROR) << "Error reading file " << path.value() << ": " << error();
      status = kStatusFileReadError;
    }
  }

  zucchini::status::Code status = kStatusSuccess;
};

}  // namespace

zucchini::status::Code MainGen(MainParams params) {
  CHECK_EQ(3U, params.file_paths->size());
  return zucchini::Generate(
      (*params.file_paths)[0], (*params.file_paths)[1], (*params.file_paths)[2],
      params.command_line->HasSwitch(kSwitchKeep),
      params.command_line->HasSwitch(kSwitchRaw),
      params.command_line->GetSwitchValueASCII(kSwitchImpose));
}

zucchini::status::Code MainApply(MainParams params) {
  CHECK_EQ(3U, params.file_paths->size());
  return zucchini::Apply((*params.file_paths)[0], (*params.file_paths)[1],
                         (*params.file_paths)[2],
                         params.command_line->HasSwitch(kSwitchKeep));
}

zucchini::status::Code MainVerify(MainParams params) {
  CHECK_EQ(1U, params.file_paths->size());
  return zucchini::VerifyPatch((*params.file_paths)[0]);
}

zucchini::status::Code MainRead(MainParams params) {
  CHECK_EQ(1U, params.file_paths->size());
  WrappedMappedFileReader input((*params.file_paths)[0]);
  if (input.status != kStatusSuccess)
    return input.status;

  bool do_dump = params.command_line->HasSwitch(kSwitchDump);
  zucchini::status::Code status = zucchini::ReadReferences(
      {input.data(), input.length()}, do_dump, *params.out);
  if (status != kStatusSuccess)
    *params.err << "Fatal error found when dumping references." << std::endl;
  return status;
}

zucchini::status::Code MainDetect(MainParams params) {
  CHECK_EQ(1U, params.file_paths->size());
  WrappedMappedFileReader input((*params.file_paths)[0]);
  if (input.status != kStatusSuccess)
    return input.status;

  std::vector<zucchini::ConstBufferView> sub_image_list;
  zucchini::status::Code result = zucchini::DetectAll(
      {input.data(), input.length()}, *params.out, &sub_image_list);
  if (result != kStatusSuccess)
    *params.err << "Fatal error found when detecting executables." << std::endl;
  return result;
}

zucchini::status::Code MainMatch(MainParams params) {
  CHECK_EQ(2U, params.file_paths->size());
  WrappedMappedFileReader old_image((*params.file_paths)[0]);
  if (old_image.status != kStatusSuccess)
    return old_image.status;
  WrappedMappedFileReader new_image((*params.file_paths)[1]);
  if (new_image.status != kStatusSuccess)
    return new_image.status;

  std::string imposed_matches =
      params.command_line->GetSwitchValueASCII(kSwitchImpose);
  zucchini::status::Code status =
      zucchini::MatchAll({old_image.data(), old_image.length()},
                         {new_image.data(), new_image.length()},
                         std::move(imposed_matches), *params.out);
  if (status != kStatusSuccess)
    *params.err << "Fatal error found when matching executables." << std::endl;
  return status;
}

zucchini::status::Code MainCrc32(MainParams params) {
  CHECK_EQ(1U, params.file_paths->size());
  WrappedMappedFileReader input((*params.file_paths)[0]);
  if (input.status != kStatusSuccess)
    return input.status;

  uint32_t crc =
      zucchini::CalculateCrc32(input.data(), input.data() + input.length());
  *params.out << "CRC32: " << zucchini::AsHex<8>(crc) << std::endl;
  return kStatusSuccess;
}

zucchini::status::Code MainSuffixArray(MainParams params) {
  CHECK_EQ(1U, params.file_paths->size());
  WrappedMappedFileReader input((*params.file_paths)[0]);
  if (input.status != kStatusSuccess)
    return input.status;

  zucchini::ImageIndex image_index(input.region());
  zucchini::EncodedView view_raw(image_index);
  std::vector<zucchini::offset_t> sa_raw =
      zucchini::MakeSuffixArray<zucchini::InducedSuffixSort>(view_raw,
                                                             size_t(256));
  return kStatusSuccess;
}
