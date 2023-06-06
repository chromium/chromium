# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# GNU Make based build file.  For details on GNU Make see:
#   http://www.gnu.org/software/make/manual/make.html
#

#
# Toolchain
#
# By default the VALID_TOOLCHAINS list contains pnacl, clang-newlib and glibc.
# If your project only builds in one or the other then this should be overridden
# accordingly.
#
ALL_TOOLCHAINS ?= pnacl glibc clang-newlib

VALID_TOOLCHAINS ?= $(ALL_TOOLCHAINS)
TOOLCHAIN ?= $(word 1,$(VALID_TOOLCHAINS))

#
# Top Make file, which we want to trigger a rebuild on if it changes
#
TOP_MAKE := $(word 1,$(MAKEFILE_LIST))


#
# Figure out which OS we are running on.
#
GETOS := python $(NACL_SDK_ROOT)/tools/getos.py
NACL_CONFIG := python $(NACL_SDK_ROOT)/tools/nacl_config.py
FIXDEPS := python $(NACL_SDK_ROOT)/tools/fix_deps.py -c
OSNAME := $(shell $(GETOS))
SYSARCH := $(shell $(GETOS) --nacl-arch)


#
# TOOLCHAIN=all recursively calls this Makefile for all VALID_TOOLCHAINS.
#
ifeq ($(TOOLCHAIN),all)

# Define the default target
all:

#
# Generate a new MAKE command for each TOOLCHAIN.
#
# Note: We use targets for each toolchain (instead of an explicit recipe) so
# each toolchain can be built in parallel.
#
# $1 = Toolchain Name
#
define TOOLCHAIN_RULE
TOOLCHAIN_TARGETS += $(1)_TARGET
.PHONY: $(1)_TARGET
$(1)_TARGET:
	+$(MAKE) TOOLCHAIN=$(1) $(MAKECMDGOALS)
endef

#
# The target for all versions
#
USABLE_TOOLCHAINS=$(filter $(OSNAME) $(ALL_TOOLCHAINS),$(VALID_TOOLCHAINS))

ifeq ($(NO_HOST_BUILDS),1)
USABLE_TOOLCHAINS:=$(filter-out $(OSNAME),$(USABLE_TOOLCHAINS))
endif

# Define the toolchain targets for all usable toolchains via the macro.
$(foreach tool,$(USABLE_TOOLCHAINS),$(eval $(call TOOLCHAIN_RULE,$(tool))))

.PHONY: all clean install
all: $(TOOLCHAIN_TARGETS)
clean: $(TOOLCHAIN_TARGETS)
install: $(TOOLCHAIN_TARGETS)

else  # TOOLCHAIN=all

#
# Verify we selected a valid toolchain for this example
#
ifeq (,$(findstring $(TOOLCHAIN),$(VALID_TOOLCHAINS)))

# Only fail to build if this is a top-level make. When building recursively, we
# don't care if an example can't build with this toolchain.
ifeq ($(MAKELEVEL),0)
  $(warning Availbile choices are: $(VALID_TOOLCHAINS))
  $(error Can not use TOOLCHAIN=$(TOOLCHAIN) on this example.)
else

# Dummy targets for recursive make with unsupported toolchain...
.PHONY: all clean install
all:
clean:
install:

endif

else  # TOOLCHAIN is valid...

#
# Build Configuration
#
# The SDK provides two sets of libraries, Debug and Release.  Debug libraries
# are compiled without optimizations to make debugging easier.  By default
# this will build a Release configuration. When debugging via "make debug",
# build the debug configuration by default instead.
#
ifneq (,$(findstring debug,$(MAKECMDGOALS)))
CONFIG ?= Debug
else
CONFIG ?= Release
endif


#
# Verify we selected a valid configuration for this example.
#
VALID_CONFIGS ?= Debug Release
ifeq (,$(findstring $(CONFIG),$(VALID_CONFIGS)))
  $(warning Availbile choices are: $(VALID_CONFIGS))
  $(error Can not use CONFIG=$(CONFIG) on this example.)
endif


#
# Note for Windows:
#   The GCC and LLVM toolchains (include the version of Make.exe that comes
# with the SDK) expect and are capable of dealing with the '/' seperator.
# For this reason, the tools in the SDK, including Makefiles and build scripts
# have a preference for POSIX style command-line arguments.
#
# Keep in mind however that the shell is responsible for command-line escaping,
# globbing, and variable expansion, so those may change based on which shell
# is used.  For Cygwin shells this can include automatic and incorrect expansion
# of response files (files starting with '@').
#
# Disable DOS PATH warning when using Cygwin based NaCl tools on Windows.
#
ifeq ($(OSNAME),win)
  # Always use cmd.exe as the shell on Windows. Otherwise Make may try to
  # search the path for sh.exe. If it is found in a path with a space, the
  # command will fail.
  SHELL := cmd.exe
  CYGWIN ?= nodosfilewarning
  export CYGWIN
endif


#
# If NACL_SDK_ROOT is not already set, then set it relative to this makefile.
#
THIS_MAKEFILE := $(CURDIR)/$(lastword $(MAKEFILE_LIST))
NACL_SDK_ROOT ?= $(realpath $(dir $(THIS_MAKEFILE))/..)


#
# Check that NACL_SDK_ROOT is set to a valid location.
# We use the existence of tools/oshelpers.py to verify the validity of the SDK
# root.
#
ifeq (,$(wildcard $(NACL_SDK_ROOT)/tools/oshelpers.py))
  $(error NACL_SDK_ROOT is set to an invalid location: $(NACL_SDK_ROOT))
endif


#
# If this makefile is part of a valid nacl SDK, but NACL_SDK_ROOT is set
# to a different location this is almost certainly a local configuration
# error.
#
LOCAL_ROOT := $(realpath $(dir $(THIS_MAKEFILE))/..)
ifneq (,$(wildcard $(LOCAL_ROOT)/tools/oshelpers.py))
  ifneq ($(realpath $(NACL_SDK_ROOT)), $(realpath $(LOCAL_ROOT)))
    $(error common.mk included from an SDK that does not match the current NACL_SDK_ROOT)
  endif
endif


#
# Alias for standard POSIX file system commands
#
OSHELPERS = python $(NACL_SDK_ROOT)/tools/oshelpers.py
WHICH := $(OSHELPERS) which
ifdef V
RM := $(OSHELPERS) rm
CP := $(OSHELPERS) cp
MKDIR := $(OSHELPERS) mkdir
MV := $(OSHELPERS) mv
else
RM := @$(OSHELPERS) rm
CP := @$(OSHELPERS) cp
MKDIR := @$(OSHELPERS) mkdir
MV := @$(OSHELPERS) mv
endif



#
# Compute path to requested NaCl Toolchain
#
TC_PATH := $(abspath $(NACL_SDK_ROOT)/toolchain)


#
# Check for required minimum SDK version.
# A makefile can declare NACL_SDK_VERSION_MIN of the form "<major>.<position>",
# where <major> is the major Chromium version number, and <position> is the
# Chromium Cr-Commit-Position number. eg. "39.295386".
#
ifdef NACL_SDK_VERSION_MIN
  VERSION_CHECK:=$(shell $(GETOS) --check-version=$(NACL_SDK_VERSION_MIN) 2>&1)
  ifneq ($(VERSION_CHECK),)
    $(error $(VERSION_CHECK))
  endif
endif


#
# The default target
#
# If no targets are specified on the command-line, the first target listed in
# the makefile becomes the default target.  By convention this is usually called
# the 'all' target.  Here we leave it blank to be first, but define it later
#
all:
.PHONY: all


#
# The install target is used to install built libraries to their final destination.
# By default this is the NaCl SDK 'lib' folder.
#
install:
.PHONY: install

ifdef SEL_LDR
STANDALONE = 1
endif

OUTBASE ?= .
CONFIG_DIR := $(CONFIG)
ifdef STANDALONE
CONFIG_DIR := standalone_$(CONFIG_DIR)
endif
ifdef MSAN
CONFIG_DIR := msan_$(CONFIG_DIR)
endif
ifdef TSAN
CONFIG_DIR := tsan_$(CONFIG_DIR)
endif
ifdef ASAN
CONFIG_DIR := asan_$(CONFIG_DIR)
endif

OUTDIR := $(OUTBASE)/$(TOOLCHAIN)/$(CONFIG_DIR)
STAMPDIR ?= $(OUTDIR)
LIBDIR ?= $(NACL_SDK_ROOT)/lib


#
# Target to remove temporary files
#
.PHONY: clean
clean:
	$(RM) -f $(TARGET).nmf
	$(RM) -rf $(OUTDIR)
	$(RM) -rf user-data-dir


#
# Rules for output directories.
#
# Output will be places in a directory name based on Toolchain and configuration
# be default this will be "newlib/Debug".  We use a python wrapped MKDIR to
# proivde a cross platform solution. The use of '|' checks for existence instead
# of timestamp, since the directory can update when files change.
#
%dir.stamp :
	$(MKDIR) -p $(dir $@)
	@echo Directory Stamp > $@


#
# Dependency Macro
#
# $1 = Name of stamp
# $2 = Directory for the sub-make
# $3 = Extra Settings
#
define DEPEND_RULE
ifndef IGNORE_DEPS
.PHONY: rebuild_$(1)

rebuild_$(1) :| $(STAMPDIR)/dir.stamp
ifeq (,$(2))
	+$(MAKE) -C $(NACL_SDK_ROOT)/src/$(1) STAMPDIR=$(abspath $(STAMPDIR)) $(abspath $(STAMPDIR)/$(1).stamp) $(3)
else
	+$(MAKE) -C $(2) STAMPDIR=$(abspath $(STAMPDIR)) $(abspath $(STAMPDIR)/$(1).stamp) $(3)
endif

all: rebuild_$(1)
$(STAMPDIR)/$(1).stamp: rebuild_$(1)

else

.PHONY: $(STAMPDIR)/$(1).stamp
$(STAMPDIR)/$(1).stamp:
	@echo Ignore $(1)
endif
endef

ifeq ($(TOOLCHAIN),win)
ifdef STANDALONE
HOST_EXT = .exe
else
HOST_EXT = .dll
endif
else
ifdef STANDALONE
HOST_EXT =
else
HOST_EXT = .so
endif
endif


#
# Common Compile Options
#
# For example, -DNDEBUG is added to release builds by default
# so that calls to assert(3) are not included in the build.
#
ifeq ($(CONFIG),Release)
POSIX_CFLAGS ?= -g -O2 -pthread -MMD -DNDEBUG
NACL_LDFLAGS ?= -O2
PNACL_LDFLAGS ?= -O2
else
POSIX_CFLAGS ?= -g -O0 -pthread -MMD -DNACL_SDK_DEBUG
endif

NACL_CFLAGS ?= -Wno-long-long -Werror
NACL_CXXFLAGS ?= -Wno-long-long -Werror
NACL_LDFLAGS += -Wl,-as-needed -pthread

#
# Default Paths
#
INC_PATHS := $(shell $(NACL_CONFIG) -t $(TOOLCHAIN) --include-dirs) $(EXTRA_INC_PATHS)
LIB_PATHS := $(NACL_SDK_ROOT)/lib $(EXTRA_LIB_PATHS)

#
# Define a LOG macro that allow a command to be run in quiet mode where
# the command echoed is not the same as the actual command executed.
# The primary use case for this is to avoid echoing the full compiler
# and linker command in the default case.  Defining V=1 will restore
# the verbose behavior
#
# $1 = The name of the tool being run
# $2 = The target file being built
# $3 = The full command to run
#
ifdef V
define LOG
$(3)
endef
else
ifeq ($(OSNAME),win)
define LOG
@echo   $(1) $(2) && $(3)
endef
else
define LOG
@echo "  $(1) $(2)" && $(3)
endef
endif
endif


#
# Convert a source path to a object file path.
# If source path is absolute then just use the basename of for the object
# file name (absolute sources paths with the same basename are not allowed).
# For relative paths use the full path to the source in the object file path
# name.
#
# $1 = Source Name
# $2 = Arch suffix
#
define SRC_TO_OBJ
$(if $(filter /%,$(1)), $(OUTDIR)/$(basename $(notdir $(1)))$(2).o, $(OUTDIR)/$(basename $(subst ..,__,$(1)))$(2).o)
endef


#
# Convert a source path to a dependency file path.
# We use the .deps extension for dependencies.  These files are generated by
# fix_deps.py based on the .d files which gcc generates.  We don't reference
# the .d files directly so that we can avoid the the case where the compile
# failed but still generated a .d file (in that case the .d file would not
# be processed by fix_deps.py)
#
# $1 = Source Name
# $2 = Arch suffix
#
define SRC_TO_DEP
$(patsubst %.o,%.deps,$(call SRC_TO_OBJ,$(1),$(2)))
endef

#
# The gcc-generated deps files end in .d
#
define SRC_TO_DEP_PRE_FIXUP
$(patsubst %.o,%.d,$(call SRC_TO_OBJ,$(1),$(2)))
endef


#
# If the requested toolchain is a NaCl or PNaCl toolchain, the use the
# macros and targets defined in nacl.mk, otherwise use the host sepecific
# macros and targets.
#
ifneq (,$(findstring $(TOOLCHAIN),linux mac))
include $(NACL_SDK_ROOT)/tools/host_gcc.mk
endif

ifneq (,$(findstring $(TOOLCHAIN),win))
include $(NACL_SDK_ROOT)/tools/host_vc.mk
endif

ifneq (,$(findstring $(TOOLCHAIN),glibc clang-newlib))
include $(NACL_SDK_ROOT)/tools/nacl_gcc.mk
endif

ifneq (,$(findstring $(TOOLCHAIN),pnacl))
include $(NACL_SDK_ROOT)/tools/nacl_llvm.mk
endif

#
# File to redirect to to in order to hide output.
#
ifeq ($(OSNAME),win)
DEV_NULL = nul
else
DEV_NULL = /dev/null
endif


#
# Variables for running examples with Chrome.
#
RUN_PY := python $(NACL_SDK_ROOT)/tools/run.py
HTTPD_PY := python $(NACL_SDK_ROOT)/tools/httpd.py

# Add this to launch Chrome with additional environment variables defined.
# Each element should be specified as KEY=VALUE, with whitespace separating
# key-value pairs. e.g.
# CHROME_ENV=FOO=1 BAR=2 BAZ=3
CHROME_ENV ?=

# Additional arguments to pass to Chrome.
CHROME_ARGS += --enable-nacl --enable-pnacl --no-first-run
CHROME_ARGS += --user-data-dir=$(CURDIR)/user-data-dir


# Paths to Debug and Release versions of the Host Pepper plugins
PPAPI_DEBUG = $(abspath $(OSNAME)/Debug/$(TARGET)$(HOST_EXT));application/x-ppapi-debug
PPAPI_RELEASE = $(abspath $(OSNAME)/Release/$(TARGET)$(HOST_EXT));application/x-ppapi-release


SEL_LDR_PATH := python $(NACL_SDK_ROOT)/tools/sel_ldr.py

ifndef STANDALONE
#
# Assign a sensible default to CHROME_PATH.
#
CHROME_PATH ?= $(shell $(GETOS) --chrome 2> $(DEV_NULL))

#
# Verify we can find the Chrome executable if we need to launch it.
#

NULL :=
SPACE := $(NULL) # one space after NULL is required
CHROME_PATH_ESCAPE := $(subst $(SPACE),\ ,$(CHROME_PATH))

ifeq ($(OSNAME),win)
  SANDBOX_ARGS := --no-sandbox
endif

GDB_PATH := $(shell $(NACL_CONFIG) -t $(TOOLCHAIN) --tool=gdb)

.PHONY: check_for_chrome
check_for_chrome:
ifeq (,$(wildcard $(CHROME_PATH_ESCAPE)))
	$(warning No valid Chrome found at CHROME_PATH=$(CHROME_PATH))
	$(error Set CHROME_PATH via an environment variable, or command-line.)
else
	$(warning Using chrome at: $(CHROME_PATH))
endif
PAGE ?= index.html
PAGE_TC_CONFIG ?= "$(PAGE)?tc=$(TOOLCHAIN)&config=$(CONFIG)"

.PHONY: run
run: check_for_chrome all $(PAGE)
	$(RUN_PY) -C $(CURDIR) -P $(PAGE_TC_CONFIG) \
	    $(addprefix -E ,$(CHROME_ENV)) -- "$(CHROME_PATH)" \
	    $(CHROME_ARGS) \
	    --allow-command-line-plugins \
	    --register-pepper-plugins="$(PPAPI_DEBUG),$(PPAPI_RELEASE)"

.PHONY: run_package
run_package: check_for_chrome all
	@echo "$(TOOLCHAIN) $(CONFIG)" > $(CURDIR)/run_package_config
	"$(CHROME_PATH)" --load-and-launch-app=$(CURDIR) $(CHROME_ARGS)

GDB_ARGS += -D $(GDB_PATH)
# PNaCl's nexe is acquired with "remote get nexe <path>" instead of the NMF.
ifeq (,$(findstring $(TOOLCHAIN),pnacl))
GDB_ARGS += -D --eval-command="nacl-manifest $(abspath $(OUTDIR))/$(TARGET).nmf"
GDB_ARGS += -D $(GDB_DEBUG_TARGET)
endif

.PHONY: debug
debug: check_for_chrome all $(PAGE)
	$(RUN_PY) $(GDB_ARGS) \
	    -C $(CURDIR) -P $(PAGE_TC_CONFIG) \
	    $(addprefix -E ,$(CHROME_ENV)) -- "$(CHROME_PATH)" \
	    $(CHROME_ARGS) $(SANDBOX_ARGS) --enable-nacl-debug \
	    --allow-command-line-plugins \
	    --register-pepper-plugins="$(PPAPI_DEBUG),$(PPAPI_RELEASE)"

.PHONY: serve
serve: all
	$(HTTPD_PY) -C $(CURDIR)
endif

# uppercase aliases (for backward compatibility)
.PHONY: CHECK_FOR_CHROME DEBUG LAUNCH RUN
CHECK_FOR_CHROME: check_for_chrome
DEBUG: debug
LAUNCH: run
RUN: run

endif  # TOOLCHAIN is valid...

endif  # TOOLCHAIN=all
