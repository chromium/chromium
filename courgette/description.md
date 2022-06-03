Courgette Internals
===================

Patch Generation
----------------

![Patch Generation](generation.png)

- courgette\_tool.cc:GenerateEnsemblePatch kicks off the patch
  generation by calling ensemble\_create.cc:GenerateEnsemblePatch

- The files are read in by in courgette:SourceStream objects

- ensemble\_create.cc:GenerateEnsemblePatch uses FindGenerators, which
  uses MakeGenerator to create
  patch\_generator\_x86\_32.h:PatchGeneratorX86\_32 classes.

- PatchGeneratorX86\_32's Transform method transforms the input file
  using Courgette's core techniques that make the bsdiff delta
  smaller.  The steps it takes are the following:

  - _disassemble_ the old and new binaries into AssemblyProgram
    objects,

  - _adjust_ the new AssemblyProgram object, and

  - _encode_ the AssemblyProgram object back into raw bytes.

### Disassemble

- The input is a pointer to a buffer containing the raw bytes of the
  input file.

- Disassembly converts certain machine instructions that reference
  addresses to Courgette instructions.  It is not actually
  disassembly, but this is the term the code-base uses.  Specifically,
  it detects instructions that use absolute addresses given by the
  binary file's relocation table, and relative addresses used in
  relative branches.

- Done by disassemble:ParseDetectedExecutable, which selects the
  appropriate Disassembler subclass by looking at the binary file's
  headers.

  - disassembler\_win32\_x86.h defines the PE/COFF x86 disassembler

  - disassembler\_elf\_32\_x86.h defines the ELF 32-bit x86 disassembler

- The Disassembler replaces the relocation table with a Courgette
  instruction that can regenerate the relocation table.

- The Disassembler builds a list of addresses referenced by the
  machine code, numbering each one.

- The Disassembler replaces and address used in machine instructions
  with its index number.

- The output is an assembly\_program.h:AssemblyProgram class, which
  contains a list of instructions, machine or Courgette, and a mapping
  of indices to actual addresses.

### Adjust

- This step takes the AssemblyProgram for the old file and reassigns
  the indices that map to actual addresses.  It is performed by
  adjustment_method.cc:Adjust().

- The goal is the match the indices from the old program to the new
  program as closely as possible.

- When matched correctly, machine instructions that jump to the
  function in both the new and old binary will look the same to
  bsdiff, even the function is located in a different part of the
  binary.

### Encode

- This step takes an AssemblyProgram object and encodes both the
  instructions and the mapping of indices to addresses as byte
  vectors.  This format can be written to a file directly, and is also
  more appropriate for bsdiffing.  It is done by
  AssemblyProgram.Encode().

- encoded_program.h:EncodedProgram defines the binary format and a
  WriteTo method that writes to a file.

### bsdiff

- simple_delta.c:GenerateSimpleDelta

Patch Application
-----------------

![Patch Application](application.png)

- courgette\_tool.cc:ApplyEnsemblePatch kicks off the patch generation
  by calling ensemble\_apply.cc:ApplyEnsemblePatch

- ensemble\_create.cc:ApplyEnsemblePatch, reads and verifies the
  patch's header, then calls the overloaded version of
  ensemble\_create.cc:ApplyEnsemblePatch.

- The patch is read into an ensemble_apply.cc:EnsemblePatchApplication
  object, which generates a set of patcher_x86_32.h:PatcherX86_32
  objects for the sections in the patch.

- The original file is disassembled and encoded via a call
  EnsemblePatchApplication.TransformUp, which in turn call
  patcher_x86_32.h:PatcherX86_32.Transform.

- The transformed file is then bspatched via
  EnsemblePatchApplication.SubpatchTransformedElements, which calls
  EnsemblePatchApplication.SubpatchStreamSets, which calls
  simple_delta.cc:ApplySimpleDelta, Courgette's built-in
  implementation of bspatch.

- Finally, EnsemblePatchApplication.TransformDown assembles, i.e.,
  reverses the encoding and disassembly, on the patched binary data.
  This is done by calling PatcherX86_32.Reform, which in turn calls
  the global function encoded_program.cc:Assemble, which calls
  EncodedProgram.AssembleTo.


Glossary
--------

**Adjust**: Reassign address indices in the new program to match more
  closely those from the old.

**Assembly program**: The output of _disassembly_.  Contains a list of
  _Courgette instructions_ and an index of branch target addresses.

**Assemble**: Convert an _assembly program_ back into an object file
  by evaluating the _Courgette instructions_ and leaving the machine
  instructions in place.

**Courgette instruction**: Replaces machine instructions in the
  program.  Courgette instructions replace branches with an index to
  the target addresses and replace part of the relocation table.

**Disassembler**: Takes a binary file and produces an _assembly
  program_.

**Encode**: Convert an _assembly program_ into an _encoded program_ by
  serializing its data structures into byte vectors more appropriate
  for storage in a file.

**Encoded Program**: The output of encoding.

**Ensemble**: A Courgette-style patch containing sections for the list
  of branch addresses, the encoded program.  It supports patching
  multiple object files at once.

**Opcode**: The number corresponding to either a machine or _Courgette
  instruction_.
