# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# GNU Make based build file.  For details on GNU Make see:
#   http://www.gnu.org/software/make/manual/make.html
#


#
# Macros for TOOLS
#
# We use the C++ compiler for everything and then use the -Wl,-as-needed flag
# in the linker to drop libc++ unless it's actually needed.
#
HOST_CC ?= python $(NACL_SDK_ROOT)/tools/cl_wrapper.py /nologo
HOST_CXX ?= python $(NACL_SDK_ROOT)/tools/cl_wrapper.py /nologo /EHsc
HOST_LINK ?= link.exe /nologo
HOST_LIB ?= lib.exe /nologo

ifeq (,$(findstring cl.exe,$(shell $(WHICH) cl.exe)))
$(warning To skip the host build use:)
$(warning "make NO_HOST_BUILDS=1")
$(error Unable to find cl.exe in PATH while building Windows host build)
endif


ifeq ($(CONFIG),Release)
WIN_OPT_FLAGS ?= /O2 /MT /Z7 -DNDEBUG
else
WIN_OPT_FLAGS ?= /Od /MTd /Z7 -DNACL_SDK_DEBUG
endif

WIN_FLAGS ?= -DWIN32 -D_WIN32 -DPTW32_STATIC_LIB


#
# Individual Macros
#
# $1 = Source Name
# $2 = Compile Flags
#
define C_COMPILER_RULE
$(call SRC_TO_OBJ,$(1)): $(1) $(TOP_MAKE) | $(dir $(call SRC_TO_OBJ,$(1)))dir.stamp
	$(call LOG,CC,$$@,$(HOST_CC) /Fo$$@ /c $$< $(WIN_OPT_FLAGS) $(2) $(WIN_FLAGS))
endef

define CXX_COMPILER_RULE
$(call SRC_TO_OBJ,$(1)): $(1) $(TOP_MAKE) | $(dir $(call SRC_TO_OBJ,$(1)))dir.stamp
	$(call LOG,CXX,$$@,$(HOST_CXX) /Fo$$@ -c $$< $(WIN_OPT_FLAGS) $(2) $(WIN_FLAGS))
endef


# $1 = Source Name
# $2 = POSIX Compile Flags (unused)
# $3 = VC Compile Flags
#
define COMPILE_RULE
ifeq ($(suffix $(1)),.c)
$(call C_COMPILER_RULE,$(1),$(3) $(foreach inc,$(INC_PATHS),/I$(inc)))
else
$(call CXX_COMPILER_RULE,$(1),$(3) $(foreach inc,$(INC_PATHS),/I$(inc)))
endif
endef


#
# LIB Macro
#
# $1 = Target Name
# $2 = List of Sources
#
#
define LIB_RULE
$(STAMPDIR)/$(1).stamp: $(LIBDIR)/$(OSNAME)_x86_32_host/$(CONFIG)/$(1).lib
	@echo "TOUCHED $$@" > $(STAMPDIR)/$(1).stamp

all:$(LIBDIR)/$(OSNAME)_x86_32_host/$(CONFIG)/$(1).lib
$(LIBDIR)/$(OSNAME)_x86_32_host/$(CONFIG)/$(1).lib: $(foreach src,$(2),$(call SRC_TO_OBJ,$(src)))
	$(MKDIR) -p $$(dir $$@)
	$(call LOG,LIB,$$@,$(HOST_LIB) /OUT:$$@ $$^ $(WIN_LDFLAGS))
endef


#
# Link Macro
#
# $1 = Target Name
# $2 = List of inputs
# $3 = List of libs
# $4 = List of deps
# $5 = List of lib dirs
# $6 = Other Linker Args
#
define LINKER_RULE
all: $(1)
$(1): $(2) $(foreach dep,$(4),$(STAMPDIR)/$(dep).stamp)
	$(call LOG,LINK,$$@,$(HOST_LINK) /DLL /OUT:$(1) /PDB:$(1).pdb $(2) /DEBUG $(foreach path,$(5),/LIBPATH:$(path)/$(OSNAME)_x86_32_host/$(CONFIG)) $(foreach lib,$(3),$(lib).lib) $(6))
endef


#
# Link Macro
#
# $1 = Target Name
# $2 = List of Sources
# $3 = List of LIBS
# $4 = List of DEPS
# $5 = POSIX Linker Switches
# $6 = VC Linker Switches
#
define LINK_RULE
$(call LINKER_RULE,$(OUTDIR)/$(1)$(HOST_EXT),$(foreach src,$(2),$(call SRC_TO_OBJ,$(src))),$(3),$(4),$(LIB_PATHS),$(6))
endef


#
# Strip Macro
# This is a nop (copy) since visual studio already keeps debug info
# separate from the binaries
#
# $1 = Target Name
# $2 = Input Name
#
define STRIP_RULE
all: $(OUTDIR)/$(1)$(HOST_EXT)
$(OUTDIR)/$(1)$(HOST_EXT): $(OUTDIR)/$(2)$(HOST_EXT)
	$(call LOG,COPY,$$@,$(CP) $$^ $$@)
endef

all: $(LIB_LIST) $(DEPS_LIST)
