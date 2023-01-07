# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# GNU Make based build file.  For details on GNU Make see:
#   http://www.gnu.org/software/make/manual/make.html
#

ifdef ASAN
CLANG = 1
endif

ifdef TSAN
CLANG = 1
endif

ifdef MSAN
CLANG = 1
endif

#
# Macros for TOOLS
#
# We use the C++ compiler for everything and then use the -Wl,-as-needed flag
# in the linker to drop libc++ unless it's actually needed.
#
ifdef CLANG
CC = clang
CXX = clang++
endif

ifdef NACL_COMPILER_PREFIX
CC = $(NACL_COMPILER_PREFIX) $(CC)
CXX = $(NACL_COMPILER_PREFIX) $(CXX)
endif

ifeq ($(OSNAME),mac)
LINK ?= $(NACL_SDK_ROOT)/tools/mac_ld_wrapper.py $(CXX)
#AR = libtool -static -no_warning_for_no_symbols
#ARFLAGS = -o
else
LINK ?= $(CXX)
endif
AR = ar
ARFLAGS = -crs
STRIP ?= strip

ifeq (,$(findstring gcc,$(shell $(WHICH) gcc)))
$(warning To skip the host build use:)
$(warning "make all_versions NO_HOST_BUILDS=1")
$(error Unable to find gcc in PATH while building Host build)
endif

HOST_WARNINGS ?= -Wno-long-long -Wall -Werror
HOST_CFLAGS = -fPIC -pthread $(HOST_WARNINGS) -I$(NACL_SDK_ROOT)/include

ifneq ($(OSNAME),mac)
# Adding -Wl,-Bsymbolic means that symbols defined within the module are always
# used by the module, and not shadowed by symbols already loaded in, for
# example, libc.  Without this the libc symbols (or anything injected with
# LD_PRELOAD will take precedence).
# -pthread is not needed on mac (libpthread is a symlink to libSystem) and
# in fact generated a warning if passed at link time.
HOST_LDFLAGS ?= -Wl,-Map,$(OUTDIR)/$(TARGET).map -Wl,-Bsymbolic -pthread
HOST_CFLAGS += -I$(NACL_SDK_ROOT)/include/mac
else
HOST_LDFLAGS ?= -Wl,-map -Wl,$(OUTDIR)/$(TARGET).map
HOST_CFLAGS += -I$(NACL_SDK_ROOT)/include/linux
endif

ifdef ASAN
HOST_CFLAGS += -fsanitize=address
HOST_LDFLAGS += -pie -fsanitize=address
endif

ifdef TSAN
HOST_CFLAGS += -fsanitize=thread
HOST_LDFLAGS += -pie -fsanitize=thread
endif

ifdef MSAN
HOST_CFLAGS += -fsanitize=memory
HOST_LDFLAGS += -pie -fsanitize=memory
endif

#
# Individual Macros
#
# $1 = Source name
# $2 = Compile flags
#
define C_COMPILER_RULE
-include $(call SRC_TO_DEP,$(1))
$(call SRC_TO_OBJ,$(1)): $(1) $(TOP_MAKE) | $(dir $(call SRC_TO_OBJ,$(1)))dir.stamp
	$(call LOG,CC  ,$$@,$(CC) -o $$@ -c $$< $(POSIX_CFLAGS) $(HOST_CFLAGS) $(CFLAGS) $(2))
	@$(FIXDEPS) $(call SRC_TO_DEP_PRE_FIXUP,$(1))
endef

define CXX_COMPILER_RULE
-include $(call SRC_TO_DEP,$(1))
$(call SRC_TO_OBJ,$(1)): $(1) $(TOP_MAKE) | $(dir $(call SRC_TO_OBJ,$(1)))dir.stamp
	$(call LOG,CXX ,$$@,$(CXX) -o $$@ -c $$< $(POSIX_CFLAGS) $(HOST_CFLAGS) $(CXXFLAGS) $(2))
	@$(FIXDEPS) $(call SRC_TO_DEP_PRE_FIXUP,$(1))
endef

#
# Compile Macro
#
# $1 = Source name
# $2 = POSIX compiler flags
# $3 = VC compiler flags (unused)
#
define COMPILE_RULE
ifeq ($(suffix $(1)),.c)
$(call C_COMPILER_RULE,$(1),$(2) $(foreach inc,$(INC_PATHS),-I$(inc)))
else
$(call CXX_COMPILER_RULE,$(1),$(2) $(foreach inc,$(INC_PATHS),-I$(inc)))
endif
endef


#
# SO Macro
#
# $1 = Target name
# $2 = list of source files
#
define SO_RULE
$(error 'Shared libraries not supported by Host')
endef


#
# LIB Macro
#
# $1 = Target name
# $2 = List of source files
#
define LIB_RULE
$(STAMPDIR)/$(1).stamp: $(LIBDIR)/$(OSNAME)_host/$(CONFIG_DIR)/lib$(1).a
	@echo "TOUCHED $$@" > $(STAMPDIR)/$(1).stamp

all: $(LIBDIR)/$(OSNAME)_host/$(CONFIG_DIR)/lib$(1).a
$(LIBDIR)/$(OSNAME)_host/$(CONFIG_DIR)/lib$(1).a: $(foreach src,$(2),$(call SRC_TO_OBJ,$(src)))
	$(MKDIR) -p $$(dir $$@)
	$(RM) -f $$@
	$(call LOG,LIB,$$@,$(AR) $(ARFLAGS) $$@ $$^)
endef


#
# Link Macro
#
# $1 = Target name
# $2 = List of inputs
# $3 = List of libs
# $4 = List of deps
# $5 = List of lib dirs
# $6 = Linker flags
#
ifdef STANDALONE
define LINKER_RULE
all: $(1)
$(1): $(2) $(foreach dep,$(4),$(STAMPDIR)/$(dep).stamp)
	$(call LOG,LINK,$$@,$(LINK) -o $(1) $(2) $(HOST_LDFLAGS) $(LDFLAGS) $(foreach path,$(5),-L$(path)/$(OSNAME)_host/$(CONFIG_DIR) -L$(path)/$(OSNAME)_host/$(CONFIG)) $(foreach lib,$(3),-l$(lib)) $(6))
endef
else
define LINKER_RULE
all: $(1)
$(1): $(2) $(foreach dep,$(4),$(STAMPDIR)/$(dep).stamp)
	$(call LOG,LINK,$$@,$(LINK) -shared -o $(1) $(2) $(HOST_LDFLAGS) $(LDFLAGS) $(foreach path,$(5),-L$(path)/$(OSNAME)_host/$(CONFIG_DIR) -L$(path)/$(OSNAME)_host/$(CONFIG)) $(foreach lib,$(3),-l$(lib)) $(6))
endef
endif


#
# Link Macro
#
# $1 = Target Name
# $2 = List of source files
# $3 = List of LIBS
# $4 = List of DEPS
# $5 = POSIX linker flags
# $6 = VC linker flags
#
define LINK_RULE
$(call LINKER_RULE,$(OUTDIR)/$(1)$(HOST_EXT),$(foreach src,$(2),$(call SRC_TO_OBJ,$(src))),$(filter-out pthread,$(3)),$(4),$(LIB_PATHS),$(5))
endef


#
# Macro to generate linker scripts
#
# $1 = Target Name
# $2 = Static Linker Script
# $3 = Shared Linker Script
#
define LINKER_SCRIPT_RULE
$(STAMPDIR)/$(1).stamp:
	@echo "  STAMP $$@"
	@echo "TOUCHED $$@" > $(STAMPDIR)/$(1).stamp

install: $(LIBDIR)/$(OSNAME)_host/$(CONFIG)/lib$(1).a
$(LIBDIR)/$(OSNAME)_host/$(CONFIG)/lib$(1).a: $(2)
	$(MKDIR) -p $$(dir $$@)
	$(call LOG,CP  ,$$@,$(OSHELPERS) cp $$^ $$@)

install: $(LIBDIR)/$(OSNAME)_host/$(CONFIG)/lib$(1).so
$(LIBDIR)/$(OSNAME)_host/$(CONFIG)/lib$(1).so: $(3)
	$(MKDIR) -p $$(dir $$@)
	$(call LOG,CP  ,$$@,$(OSHELPERS) cp $$^ $$@)
endef


all: $(LIB_LIST) $(DEPS_LIST)


#
# Strip Macro
# The host build makes shared libraries, so the best we can do is -S, which
# only strip debug symbols.  We don't strip the symbol names.
#
# $1 = Target name
# $2 = Input name
#
define STRIP_RULE
all: $(OUTDIR)/$(1)$(HOST_EXT)
$(OUTDIR)/$(1)$(HOST_EXT): $(OUTDIR)/$(2)$(HOST_EXT)
	$(call LOG,STRIP,$$@,$(STRIP) -S -o $$@ $$^)
endef


#
# Run standalone builds (command line builds outside of chrome)
#
ifdef STANDALONE
run: all
	$(RUN_UNDER) $(OUTDIR)/$(TARGET)$(HOST_EXT) $(EXE_ARGS)
endif
