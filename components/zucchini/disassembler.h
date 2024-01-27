// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_DISASSEMBLER_H_
#define COMPONENTS_ZUCCHINI_DISASSEMBLER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "components/zucchini/buffer_view.h"
#include "components/zucchini/image_utils.h"

namespace zucchini {

// A vacuous ReferenceReader that produces no references.
class EmptyReferenceReader : public ReferenceReader {
 public:
  std::optional<Reference> GetNext() override;
};

// A vacuous EmptyReferenceWriter that does not write.
class EmptyReferenceWriter : public ReferenceWriter {
 public:
  void PutNext(Reference reference) override;
};

// Disassembler needs to be declared before ReferenceGroup because the latter
// contains member pointers based on the former, and we use a compiler flag,
// -fcomplete-member-pointers, which enforces that member pointer base types are
// complete. This flag helps prevent us from running into problems in the
// Microsoft C++ ABI (see https://crbug.com/847724).

class ReferenceGroup;

// A Disassembler is used to encapsulate architecture specific operations, to:
// - Describe types of references found in the architecture using traits.
// - Extract references contained in an image file.
// - Correct target for some references.
class Disassembler {
 public:
  // Attempts to parse |image| and create an architecture-specifc Disassembler,
  // as determined by DIS, which is inherited from Disassembler. Returns an
  // instance of DIS if successful, and null otherwise.
  template <class DIS>
  static std::unique_ptr<DIS> Make(ConstBufferView image) {
    auto disasm = std::make_unique<DIS>();
    if (!disasm->Parse(image))
      return nullptr;
    return disasm;
  }

  Disassembler(const Disassembler&) = delete;
  const Disassembler& operator=(const Disassembler&) = delete;
  virtual ~Disassembler();

  // Returns the type of executable handled by the Disassembler.
  virtual ExecutableType GetExeType() const = 0;

  // Returns a more detailed description of the executable type.
  virtual std::string GetExeTypeString() const = 0;

  // Creates and returns a vector that contains all groups of references.
  // Groups must be aggregated by pool.
  virtual std::vector<ReferenceGroup> MakeReferenceGroups() const = 0;

  ConstBufferView image() const { return image_; }
  size_t size() const { return image_.size(); }

  int num_equivalence_iterations() const { return num_equivalence_iterations_; }

 protected:
  explicit Disassembler(int num_equivalence_iterations);

  // Parses |image| and initializes internal states. Returns true on success.
  // This must be called once and before any other operation.
  virtual bool Parse(ConstBufferView image) = 0;

  // Raw image data. After Parse(), a Disassembler should shrink this to contain
  // only the portion containing the executable file it recognizes.
  ConstBufferView image_;

  // The number of iterations to run for equivalence map generation. This should
  // roughly be the max length of reference indirection chains.
  int num_equivalence_iterations_;
};

// A ReferenceGroup is associated with a specific |type| and has convenience
// methods to obtain readers and writers for that type. A ReferenceGroup does
// not store references; it is a lightweight class that communicates with the
// disassembler to operate on them.
class ReferenceGroup {
 public:
  // Member function pointer used to obtain a ReferenceReader.
  using ReaderFactory = std::unique_ptr<ReferenceReader> (
      Disassembler::*)(offset_t lower, offset_t upper);

  // Member function pointer used to obtain a ReferenceWriter.
  using WriterFactory = std::unique_ptr<ReferenceWriter> (Disassembler::*)(
      MutableBufferView image);

  // Member function pointer used to obtain a ReferenceMixer.
  using MixerFactory = std::unique_ptr<ReferenceMixer> (
      Disassembler::*)(ConstBufferView old_image, ConstBufferView new_image);

  // RefinedGeneratorFactory and RefinedReceptorFactory don't have to be
  // identical to GeneratorFactory and ReceptorFactory, but they must be
  // convertible. As a result, they can be pointer to member function of a
  // derived Disassembler.
  template <class RefinedReaderFactory, class RefinedWriterFactory>
  ReferenceGroup(ReferenceTypeTraits traits,
                 RefinedReaderFactory reader_factory,
                 RefinedWriterFactory writer_factory)
      : traits_(traits),
        reader_factory_(static_cast<ReaderFactory>(reader_factory)),
        writer_factory_(static_cast<WriterFactory>(writer_factory)) {}

  template <class RefinedReaderFactory,
            class RefinedWriterFactory,
            class RefinedMixerFactory>
  ReferenceGroup(ReferenceTypeTraits traits,
                 RefinedReaderFactory reader_factory,
                 RefinedWriterFactory writer_factory,
                 RefinedMixerFactory mixer_factory)
      : traits_(traits),
        reader_factory_(static_cast<ReaderFactory>(reader_factory)),
        writer_factory_(static_cast<WriterFactory>(writer_factory)),
        mixer_factory_(static_cast<MixerFactory>(mixer_factory)) {}

  // Returns a reader for all references in the binary.
  // Invalidates any other writer or reader previously obtained for |disasm|.
  std::unique_ptr<ReferenceReader> GetReader(Disassembler* disasm) const;

  // Returns a reader for references whose bytes are entirely contained in
  // |[lower, upper)|.
  // Invalidates any other writer or reader previously obtained for |disasm|.
  std::unique_ptr<ReferenceReader> GetReader(offset_t lower,
                                             offset_t upper,
                                             Disassembler* disasm) const;

  // Returns a writer for references in |image|, assuming that |image| was the
  // same one initially parsed by |disasm|.
  // Invalidates any other writer or reader previously obtained for |disasm|.
  std::unique_ptr<ReferenceWriter> GetWriter(MutableBufferView image,
                                             Disassembler* disasm) const;

  // Returns mixer for references between |old_image| and |new_image|, assuming
  // they both contain the same type of executable as |disasm|.
  std::unique_ptr<ReferenceMixer> GetMixer(ConstBufferView old_image,
                                           ConstBufferView new_image,
                                           Disassembler* disasm) const;

  // Returns traits describing the reference type.
  const ReferenceTypeTraits& traits() const { return traits_; }

  // Shorthand for traits().width.
  offset_t width() const { return traits().width; }

  // Shorthand for traits().type_tag.
  TypeTag type_tag() const { return traits().type_tag; }

  // Shorthand for traits().pool_tag.
  PoolTag pool_tag() const { return traits().pool_tag; }

 private:
  ReferenceTypeTraits traits_;
  ReaderFactory reader_factory_ = nullptr;
  WriterFactory writer_factory_ = nullptr;
  MixerFactory mixer_factory_ = nullptr;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_DISASSEMBLER_H_
