// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/disassembler_win32.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "courgette/assembly_program.h"
#include "courgette/courgette.h"

#if COURGETTE_HISTOGRAM_TARGETS
#include <iostream>
#endif

namespace courgette {

DisassemblerWin32::DisassemblerWin32(const uint8_t* start, size_t length)
    : Disassembler(start, length) {}

RVA DisassemblerWin32::FileOffsetToRVA(FileOffset file_offset) const {
  for (int i = 0; i < number_of_sections_; ++i) {
    const Section* section = &sections_[i];
    if (file_offset >= section->file_offset_of_raw_data) {
      FileOffset offset_in_section =
          file_offset - section->file_offset_of_raw_data;
      if (offset_in_section < section->size_of_raw_data)
        return static_cast<RVA>(section->virtual_address + offset_in_section);
    }
  }

  NOTREACHED();
  return kNoRVA;
}

FileOffset DisassemblerWin32::RVAToFileOffset(RVA rva) const {
  const Section* section = RVAToSection(rva);
  if (section != nullptr) {
    FileOffset offset_in_section = rva - section->virtual_address;
    // Need this extra check, since an |rva| may be valid for a section, but is
    // non-existent in an image (e.g. uninit data).
    if (offset_in_section >= section->size_of_raw_data)
      return kNoFileOffset;

    return static_cast<FileOffset>(section->file_offset_of_raw_data +
                                   offset_in_section);
  }

  // Small RVA values point into the file header in the loaded image.
  // RVA 0 is the module load address which Windows uses as the module handle.
  // RVA 2 sometimes occurs, I'm not sure what it is, but it would map into the
  // DOS header.
  if (rva == 0 || rva == 2)
    return static_cast<FileOffset>(rva);

  NOTREACHED();
  return kNoFileOffset;
}

// ParseHeader attempts to match up the buffer with the Windows data
// structures that exist within a Windows 'Portable Executable' format file.
// Returns 'true' if the buffer matches, and 'false' if the data looks
// suspicious.  Rather than try to 'map' the buffer to the numerous windows
// structures, we extract the information we need into the courgette::PEInfo
// structure.
//
bool DisassemblerWin32::ParseHeader() {
  if (!IsRangeInBounds(kOffsetOfFileAddressOfNewExeHeader, 4))
    return Bad("Too small");

  // Have 'MZ' magic for a DOS header?
  if (start()[0] != 'M' || start()[1] != 'Z')
    return Bad("Not MZ");

  // offset from DOS header to PE header is stored in DOS header.
  FileOffset pe_header_offset = static_cast<FileOffset>(
      ReadU32(start(), kOffsetOfFileAddressOfNewExeHeader));
  if (pe_header_offset % 8 != 0)
    return Bad("Misaligned PE header");
  if (pe_header_offset < kOffsetOfFileAddressOfNewExeHeader + 4)
    return Bad("PE header pathological overlap");
  if (!IsRangeInBounds(pe_header_offset, kMinPeHeaderSize))
    return Bad("PE header past end of file");

  const uint8_t* const pe_header = FileOffsetToPointer(pe_header_offset);

  // The 'PE' header is an IMAGE_NT_HEADERS structure as defined in WINNT.H.
  // See http://msdn.microsoft.com/en-us/library/ms680336(VS.85).aspx
  //
  // The first field of the IMAGE_NT_HEADERS is the signature.
  if (!(pe_header[0] == 'P' && pe_header[1] == 'E' && pe_header[2] == 0 &&
        pe_header[3] == 0)) {
    return Bad("No PE signature");
  }

  // The second field of the IMAGE_NT_HEADERS is the COFF header.
  // The COFF header is also called an IMAGE_FILE_HEADER
  //   http://msdn.microsoft.com/en-us/library/ms680313(VS.85).aspx
  FileOffset coff_header_offset = pe_header_offset + 4;
  if (!IsRangeInBounds(coff_header_offset, kSizeOfCoffHeader))
    return Bad("COFF header past end of file");
  const uint8_t* const coff_header = start() + coff_header_offset;
  machine_type_ = ReadU16(coff_header, 0);
  number_of_sections_ = ReadU16(coff_header, 2);
  size_of_optional_header_ = ReadU16(coff_header, 16);
  // Check we can read the magic.
  if (size_of_optional_header_ < 2)
    return Bad("Optional header no magic");
  // Check that we can read the rest of the the fixed fields. Data directories
  // directly follow the fixed fields of the IMAGE_OPTIONAL_HEADER.
  if (size_of_optional_header_ < RelativeOffsetOfDataDirectories())
    return Bad("Optional header too short");

  // The rest of the IMAGE_NT_HEADERS is the IMAGE_OPTIONAL_HEADER(32|64)
  FileOffset optional_header_offset = pe_header_offset + kMinPeHeaderSize;
  if (!IsRangeInBounds(optional_header_offset, size_of_optional_header_))
    return Bad("Optional header past end of file");
  optional_header_ = start() + optional_header_offset;

  uint16_t magic = ReadU16(optional_header_, 0);
  switch (kind()) {
    case EXE_WIN_32_X86:
      if (magic != kImageNtOptionalHdr32Magic)
        return Bad("64 bit executables are not supported by this disassembler");
      break;

    case EXE_WIN_32_X64:
      if (magic != kImageNtOptionalHdr64Magic)
        return Bad("32 bit executables are not supported by this disassembler");
      break;

    default:
      return Bad("Unrecognized magic");
  }

  // The optional header is either an IMAGE_OPTIONAL_HEADER32 or
  // IMAGE_OPTIONAL_HEADER64
  // http://msdn.microsoft.com/en-us/library/ms680339(VS.85).aspx
  //
  // Copy the fields we care about.
  size_of_code_ = ReadU32(optional_header_, 4);
  size_of_initialized_data_ = ReadU32(optional_header_, 8);
  size_of_uninitialized_data_ = ReadU32(optional_header_, 12);
  base_of_code_ = ReadU32(optional_header_, 20);

  switch (kind()) {
    case EXE_WIN_32_X86:
      base_of_data_ = ReadU32(optional_header_, 24);
      image_base_ = ReadU32(optional_header_, 28);
      size_of_image_ = ReadU32(optional_header_, 56);
      number_of_data_directories_ = ReadU32(optional_header_, 92);
      break;

    case EXE_WIN_32_X64:
      base_of_data_ = 0;
      image_base_ = ReadU64(optional_header_, 24);
      size_of_image_ = ReadU32(optional_header_, 56);
      number_of_data_directories_ = ReadU32(optional_header_, 108);
      break;

    default:
      NOTREACHED();
  }

  if (size_of_image_ >= 0x80000000U)
    return Bad("Invalid SizeOfImage");

  if (size_of_code_ >= length() || size_of_initialized_data_ >= length() ||
      size_of_code_ + size_of_initialized_data_ >= length()) {
    // This validation fires on some perfectly fine executables.
    //  return Bad("code or initialized data too big");
  }

  // TODO(sra): we can probably get rid of most of the data directories.
  bool b = true;
  // 'b &= ...' could be short circuit 'b = b && ...' but it is not necessary
  // for correctness and it compiles smaller this way.
  b &= ReadDataDirectory(0, &export_table_);
  b &= ReadDataDirectory(1, &import_table_);
  b &= ReadDataDirectory(2, &resource_table_);
  b &= ReadDataDirectory(3, &exception_table_);
  b &= ReadDataDirectory(5, &base_relocation_table_);
  b &= ReadDataDirectory(11, &bound_import_table_);
  b &= ReadDataDirectory(12, &import_address_table_);
  b &= ReadDataDirectory(13, &delay_import_descriptor_);
  b &= ReadDataDirectory(14, &clr_runtime_header_);
  if (!b)
    return Bad("Malformed data directory");

  // Sections follow the optional header.
  FileOffset sections_offset =
      optional_header_offset + size_of_optional_header_;
  if (!IsArrayInBounds(sections_offset, number_of_sections_, sizeof(Section)))
    return Bad("Sections past end of file");
  sections_ = reinterpret_cast<const Section*>(start() + sections_offset);
  if (!CheckSectionRanges())
    return Bad("Out of bound section");

  size_t detected_length = 0;
  for (int i = 0; i < number_of_sections_; ++i) {
    const Section* section = &sections_[i];

    // TODO(sra): consider using the 'characteristics' field of the section
    // header to see if the section contains instructions.
    if (memcmp(section->name, ".text", 6) == 0)
      has_text_section_ = true;

    uint32_t section_end =
        section->file_offset_of_raw_data + section->size_of_raw_data;
    if (section_end > detected_length)
      detected_length = section_end;
  }

  // Pretend our in-memory copy is only as long as our detected length.
  ReduceLength(detected_length);

  if (!has_text_section()) {
    return Bad("Resource-only executables are not yet supported");
  }

  return Good();
}

////////////////////////////////////////////////////////////////////////////////

bool DisassemblerWin32::ParseRelocs(std::vector<RVA>* relocs) {
  relocs->clear();

  size_t relocs_size = base_relocation_table_.size_;
  if (relocs_size == 0)
    return true;

  // The format of the base relocation table is a sequence of variable sized
  // IMAGE_BASE_RELOCATION blocks.  Search for
  //   "The format of the base relocation data is somewhat quirky"
  // at http://msdn.microsoft.com/en-us/library/ms809762.aspx

  const uint8_t* relocs_start = RVAToPointer(base_relocation_table_.address_);
  const uint8_t* relocs_end = relocs_start + relocs_size;

  // Make sure entire base relocation table is within the buffer.
  if (relocs_start < start() || relocs_start >= end() ||
      relocs_end <= start() || relocs_end > end()) {
    return Bad(".relocs outside image");
  }

  const uint8_t* block = relocs_start;

  // Walk the variable sized blocks.
  while (block + 8 < relocs_end) {
    RVA page_rva = ReadU32(block, 0);
    uint32_t size = ReadU32(block, 4);
    if (size < 8 ||     // Size includes header ...
        size % 4 != 0)  // ... and is word aligned.
      return Bad("Unreasonable relocs block");

    const uint8_t* end_entries = block + size;

    if (end_entries <= block || end_entries <= start() || end_entries > end())
      return Bad(".relocs block outside image");

    // Walk through the two-byte entries.
    for (const uint8_t* p = block + 8; p < end_entries; p += 2) {
      uint16_t entry = ReadU16(p, 0);
      int type = entry >> 12;
      int offset = entry & 0xFFF;

      RVA rva = page_rva + offset;
      // Skip the relocs that live outside of the image. It might be the case
      // if a reloc is relative to a register, e.g.:
      //     mov    ecx,dword ptr [eax+044D5888h]
      RVA target_rva = PointerToTargetRVA(RVAToPointer(rva));
      if (target_rva == kNoRVA) {
        continue;
      }

      if (SupportsRelTableType(type)) {
        relocs->push_back(rva);
      } else if (type == 0) {  // IMAGE_REL_BASED_ABSOLUTE
        // Ignore, used as padding.
      } else {
        // Does not occur in Windows x86/x64 executables.
        return Bad("Unknown type of reloc");
      }
    }

    block += size;
  }

  std::sort(relocs->begin(), relocs->end());
  DCHECK(relocs->empty() || relocs->back() != kUnassignedRVA);

  return true;
}

const Section* DisassemblerWin32::RVAToSection(RVA rva) const {
  for (int i = 0; i < number_of_sections_; ++i) {
    const Section* section = &sections_[i];
    if (rva >= section->virtual_address) {
      FileOffset offset_in_section = rva - section->virtual_address;
      if (offset_in_section < section->virtual_size)
        return section;
    }
  }
  return nullptr;
}

std::string DisassemblerWin32::SectionName(const Section* section) {
  if (section == nullptr)
    return "<none>";
  char name[9];
  memcpy(name, section->name, 8);
  name[8] = '\0';  // Ensure termination.
  return name;
}

// static
bool DisassemblerWin32::QuickDetect(const uint8_t* start,
                                    size_t length,
                                    uint16_t magic) {
  if (length < kOffsetOfFileAddressOfNewExeHeader + 4)
    return false;

  // Have 'MZ' magic for a DOS header?
  if (start[0] != 'M' || start[1] != 'Z')
    return false;

  FileOffset pe_header_offset = static_cast<FileOffset>(
      ReadU32(start, kOffsetOfFileAddressOfNewExeHeader));
  if (pe_header_offset % 8 != 0 ||
      pe_header_offset < kOffsetOfFileAddressOfNewExeHeader + 4 ||
      pe_header_offset >= length ||
      length - pe_header_offset < kMinPeHeaderSize) {
    return false;
  }
  const uint8_t* pe_header = start + pe_header_offset;
  if (!(pe_header[0] == 'P' && pe_header[1] == 'E' && pe_header[2] == 0 &&
        pe_header[3] == 0)) {
    return false;
  }

  FileOffset optional_header_offset = pe_header_offset + kMinPeHeaderSize;
  if (optional_header_offset >= length || length - optional_header_offset < 2)
    return false;
  const uint8_t* optional_header = start + optional_header_offset;
  return magic == ReadU16(optional_header, 0);
}

bool DisassemblerWin32::IsRvaRangeInBounds(size_t start, size_t length) {
  return start < size_of_image_ && length <= size_of_image_ - start;
}

bool DisassemblerWin32::CheckSectionRanges() {
  for (int i = 0; i < number_of_sections_; ++i) {
    const Section* section = &sections_[i];
    if (!IsRangeInBounds(section->file_offset_of_raw_data,
                         section->size_of_raw_data) ||
        !IsRvaRangeInBounds(section->virtual_address, section->virtual_size)) {
      return false;
    }
  }
  return true;
}

bool DisassemblerWin32::ExtractAbs32Locations() {
  abs32_locations_.clear();
  if (!ParseRelocs(&abs32_locations_))
    return false;

#if COURGETTE_HISTOGRAM_TARGETS
  for (size_t i = 0; i < abs32_locations_.size(); ++i) {
    RVA rva = abs32_locations_[i];
    // The 4 bytes at the relocation are a reference to some address.
    ++abs32_target_rvas_[PointerToTargetRVA(RVAToPointer(rva))];
  }
#endif
  return true;
}

bool DisassemblerWin32::ExtractRel32Locations() {
  FileOffset file_offset = 0;
  while (file_offset < length()) {
    const Section* section = FindNextSection(file_offset);
    if (section == nullptr)
      break;
    if (file_offset < section->file_offset_of_raw_data)
      file_offset = section->file_offset_of_raw_data;
    ParseRel32RelocsFromSection(section);
    file_offset += section->size_of_raw_data;
  }
  std::sort(rel32_locations_.begin(), rel32_locations_.end());
  DCHECK(rel32_locations_.empty() || rel32_locations_.back() != kUnassignedRVA);

#if COURGETTE_HISTOGRAM_TARGETS
  VLOG(1) << "abs32_locations_ " << abs32_locations_.size()
          << "\nrel32_locations_ " << rel32_locations_.size()
          << "\nabs32_target_rvas_ " << abs32_target_rvas_.size()
          << "\nrel32_target_rvas_ " << rel32_target_rvas_.size();

  int common = 0;
  std::map<RVA, int>::iterator abs32_iter = abs32_target_rvas_.begin();
  std::map<RVA, int>::iterator rel32_iter = rel32_target_rvas_.begin();
  while (abs32_iter != abs32_target_rvas_.end() &&
         rel32_iter != rel32_target_rvas_.end()) {
    if (abs32_iter->first < rel32_iter->first) {
      ++abs32_iter;
    } else if (rel32_iter->first < abs32_iter->first) {
      ++rel32_iter;
    } else {
      ++common;
      ++abs32_iter;
      ++rel32_iter;
    }
  }
  VLOG(1) << "common " << common;
#endif
  return true;
}

RvaVisitor* DisassemblerWin32::CreateAbs32TargetRvaVisitor() {
  return new RvaVisitor_Abs32(abs32_locations_, *this);
}

RvaVisitor* DisassemblerWin32::CreateRel32TargetRvaVisitor() {
  return new RvaVisitor_Rel32(rel32_locations_, *this);
}

void DisassemblerWin32::RemoveUnusedRel32Locations(
    AssemblyProgram* program) {
  auto cond = [this, program](RVA rva) -> bool {
    // + 4 since offset is relative to start of next instruction.
    RVA target_rva = rva + 4 + Read32LittleEndian(RVAToPointer(rva));
    return program->FindRel32Label(target_rva) == nullptr;
  };
  rel32_locations_.erase(
      std::remove_if(rel32_locations_.begin(), rel32_locations_.end(), cond),
      rel32_locations_.end());
}

InstructionGenerator DisassemblerWin32::GetInstructionGenerator(
    AssemblyProgram* program) {
  return base::BindRepeating(&DisassemblerWin32::ParseFile,
                             base::Unretained(this), program);
}

CheckBool DisassemblerWin32::ParseFile(AssemblyProgram* program,
                                       InstructionReceptor* receptor) const {
  // Walk all the bytes in the file, whether or not in a section.
  FileOffset file_offset = 0;
  while (file_offset < length()) {
    const Section* section = FindNextSection(file_offset);
    if (section == nullptr) {
      // No more sections. There should not be extra stuff following last
      // section.
      //   ParseNonSectionFileRegion(file_offset, pe_info().length(), receptor);
      break;
    }
    if (file_offset < section->file_offset_of_raw_data) {
      FileOffset section_start_offset = section->file_offset_of_raw_data;
      if (!ParseNonSectionFileRegion(file_offset, section_start_offset,
                                     receptor)) {
        return false;
      }

      file_offset = section_start_offset;
    }
    FileOffset end = file_offset + section->size_of_raw_data;
    if (!ParseFileRegion(section, file_offset, end, program, receptor))
      return false;
    file_offset = end;
  }

#if COURGETTE_HISTOGRAM_TARGETS
  HistogramTargets("abs32 relocs", abs32_target_rvas_);
  HistogramTargets("rel32 relocs", rel32_target_rvas_);
#endif

  return true;
}

CheckBool DisassemblerWin32::ParseNonSectionFileRegion(
    FileOffset start_file_offset,
    FileOffset end_file_offset,
    InstructionReceptor* receptor) const {
  if (incomplete_disassembly_)
    return true;

  if (end_file_offset > start_file_offset) {
    if (!receptor->EmitMultipleBytes(FileOffsetToPointer(start_file_offset),
                                     end_file_offset - start_file_offset)) {
      return false;
    }
  }

  return true;
}

CheckBool DisassemblerWin32::ParseFileRegion(
    const Section* section,
    FileOffset start_file_offset,
    FileOffset end_file_offset,
    AssemblyProgram* program,
    InstructionReceptor* receptor) const {
  RVA relocs_start_rva = base_relocation_table().address_;

  const uint8_t* start_pointer = FileOffsetToPointer(start_file_offset);
  const uint8_t* end_pointer = FileOffsetToPointer(end_file_offset);

  RVA start_rva = FileOffsetToRVA(start_file_offset);
  RVA end_rva = start_rva + section->virtual_size;
  const int kVAWidth = AbsVAWidth();

  // Quick way to convert from Pointer to RVA within a single Section is to
  // subtract 'pointer_to_rva'.
  const uint8_t* const adjust_pointer_to_rva = start_pointer - start_rva;

  std::vector<RVA>::const_iterator rel32_pos = rel32_locations_.begin();
  std::vector<RVA>::const_iterator abs32_pos = abs32_locations_.begin();

  if (!receptor->EmitOrigin(start_rva))
    return false;

  const uint8_t* p = start_pointer;

  while (p < end_pointer) {
    RVA current_rva = static_cast<RVA>(p - adjust_pointer_to_rva);

    // The base relocation table is usually in the .relocs section, but it could
    // actually be anywhere.  Make sure we skip it because we will regenerate it
    // during assembly.
    if (current_rva == relocs_start_rva) {
      if (!receptor->EmitPeRelocs())
        return false;
      uint32_t relocs_size = base_relocation_table().size_;
      if (relocs_size) {
        p += relocs_size;
        continue;
      }
    }

    while (abs32_pos != abs32_locations_.end() && *abs32_pos < current_rva)
      ++abs32_pos;

    if (abs32_pos != abs32_locations_.end() && *abs32_pos == current_rva) {
      RVA target_rva = PointerToTargetRVA(p);
      DCHECK_NE(kNoRVA, target_rva);
      // TODO(sra): target could be Label+offset.  It is not clear how to guess
      // which it might be.  We assume offset==0.
      Label* label = program->FindAbs32Label(target_rva);
      DCHECK(label);
      if (!EmitAbs(label, receptor))
        return false;
      p += kVAWidth;
      continue;
    }

    while (rel32_pos != rel32_locations_.end() && *rel32_pos < current_rva)
      ++rel32_pos;

    if (rel32_pos != rel32_locations_.end() && *rel32_pos == current_rva) {
      // + 4 since offset is relative to start of next instruction.
      RVA target_rva = current_rva + 4 + Read32LittleEndian(p);
      Label* label = program->FindRel32Label(target_rva);
      DCHECK(label);
      if (!receptor->EmitRel32(label))
        return false;
      p += 4;
      continue;
    }

    if (incomplete_disassembly_) {
      if ((abs32_pos == abs32_locations_.end() || end_rva <= *abs32_pos) &&
          (rel32_pos == rel32_locations_.end() || end_rva <= *rel32_pos) &&
          (end_rva <= relocs_start_rva || current_rva >= relocs_start_rva)) {
        // No more relocs in this section, don't bother encoding bytes.
        break;
      }
    }

    if (!receptor->EmitSingleByte(*p))
      return false;
    p += 1;
  }

  return true;
}

#if COURGETTE_HISTOGRAM_TARGETS
// Histogram is printed to std::cout.  It is purely for debugging the algorithm
// and is only enabled manually in 'exploration' builds.  I don't want to add
// command-line configuration for this feature because this code has to be
// small, which means compiled-out.
void DisassemblerWin32::HistogramTargets(const char* kind,
                                         const std::map<RVA, int>& map) const {
  int total = 0;
  std::map<int, std::vector<RVA>> h;
  for (std::map<RVA, int>::const_iterator p = map.begin(); p != map.end();
       ++p) {
    h[p->second].push_back(p->first);
    total += p->second;
  }

  std::cout << total << " " << kind << " to " << map.size() << " unique targets"
            << std::endl;

  std::cout << "indegree: #targets-with-indegree (example)" << std::endl;
  const int kFirstN = 15;
  bool someSkipped = false;
  int index = 0;
  for (std::map<int, std::vector<RVA>>::reverse_iterator p = h.rbegin();
       p != h.rend(); ++p) {
    ++index;
    if (index <= kFirstN || p->first <= 3) {
      if (someSkipped) {
        std::cout << "..." << std::endl;
      }
      size_t count = p->second.size();
      std::cout << std::dec << p->first << ": " << count;
      if (count <= 2) {
        for (size_t i = 0; i < count; ++i)
          std::cout << "  " << DescribeRVA(p->second[i]);
      }
      std::cout << std::endl;
      someSkipped = false;
    } else {
      someSkipped = true;
    }
  }
}
#endif  // COURGETTE_HISTOGRAM_TARGETS

// DescribeRVA is for debugging only.  I would put it under #ifdef DEBUG except
// that during development I'm finding I need to call it when compiled in
// Release mode.  Hence:
// TODO(sra): make this compile only for debug mode.
std::string DisassemblerWin32::DescribeRVA(RVA rva) const {
  const Section* section = RVAToSection(rva);
  std::ostringstream s;
  s << std::hex << rva;
  if (section) {
    s << " (";
    s << SectionName(section) << "+" << std::hex
      << (rva - section->virtual_address) << ")";
  }
  return s.str();
}

const Section* DisassemblerWin32::FindNextSection(
    FileOffset file_offset) const {
  const Section* best = nullptr;
  for (int i = 0; i < number_of_sections_; ++i) {
    const Section* section = &sections_[i];
    if (section->size_of_raw_data > 0) {  // i.e. has data in file.
      if (file_offset <= section->file_offset_of_raw_data) {
        if (best == nullptr ||
            section->file_offset_of_raw_data < best->file_offset_of_raw_data) {
          best = section;
        }
      }
    }
  }
  return best;
}

bool DisassemblerWin32::ReadDataDirectory(int index,
                                          ImageDataDirectory* directory) {
  if (index < number_of_data_directories_) {
    FileOffset file_offset = index * 8 + RelativeOffsetOfDataDirectories();
    if (file_offset >= size_of_optional_header_)
      return Bad("Number of data directories inconsistent");
    const uint8_t* data_directory = optional_header_ + file_offset;
    if (data_directory < start() || data_directory + 8 >= end())
      return Bad("Data directory outside image");
    RVA rva = ReadU32(data_directory, 0);
    size_t size = ReadU32(data_directory, 4);
    if (size > size_of_image_)
      return Bad("Data directory size too big");

    // TODO(sra): validate RVA.
    directory->address_ = rva;
    directory->size_ = static_cast<uint32_t>(size);
    return true;
  } else {
    directory->address_ = 0;
    directory->size_ = 0;
    return true;
  }
}

}  // namespace courgette
