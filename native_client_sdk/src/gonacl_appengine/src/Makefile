# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# This script allows you to build, upload, and publish demo binaries that will
# go on Google Cloud Storage.
#
# NOTE: you normally should not upload examples. This will be done
# automatically by the buildbots.
#
# Building
# --------
#
# * Build all examples.
#
#     $ make
#
# * Build a specific example.
#
#     $ make bullet
#
# Uploading
# ---------
#
# * Build and upload all examples.
#
#     $ make upload REVISION=1234
#
# * Build and upload a single example
#
#     $ make upload-bullet REVISION=1234
#
# Publishing
# ----------
#
# * Publish binaries for all examples.
#
#     $ make publish REVISION=1234
#
# * Publish binaries for one example.
#
#     $ make publish-bullet REVISION=1234
#

ifeq (,$(NACL_SDK_ROOT))
  $(error NACL_SDK_ROOT is not defined)
endif

# Define default build target
all:

#
# All projects built by this Makefile
#
PROJECTS = earth voronoi life bullet lua smoothlife cube

GS_URL_CONTINUOUS = gs://gonacl/demos/continuous
GS_URL_PUBLISH = gs://gonacl/demos/publish

#
# Each project must define the following variables. All paths should be
# absolute paths.
#
# foo_SRCS: src files for this project
# foo_TGTS: generated files for this project
# foo_EXTRA_UPLOADS: additional files to upload for this project (optional)
#
# And a recipe to build TGTS from SRCS:
# $(foo_TGTS): $(foo_SRCS)
# 	...
#

#
# Earth
#
earth_SRC_DIR = earth
earth_TGT_DIR = earth/pnacl/Release
earth_JPG_DIR = ../../examples/demo/earth
earth_SRCS = $(earth_SRC_DIR)/earth.cc \
             $(earth_SRC_DIR)/Makefile \
             common/fps.h
earth_TGTS = $(earth_TGT_DIR)/earth.pexe \
             $(earth_TGT_DIR)/earth.nmf
earth_JPGS = $(earth_JPG_DIR)/earth.jpg \
             $(earth_JPG_DIR)/earthnight.jpg
earth_EXTRA_UPLOADS = $(earth_JPGS)

$(earth_TGTS): $(earth_SRCS)
	$(MAKE) -C earth TOOLCHAIN=pnacl CONFIG=Release


#
# Bullet
#
bullet_SRC_DIR = bullet
bullet_TGT_DIR = bullet/out
bullet_SRCS = $(bullet_SRC_DIR)/build.sh \
              $(bullet_SRC_DIR)/Makefile
bullet_TGTS = $(bullet_TGT_DIR)/NaClAMBullet.pexe \
              $(bullet_TGT_DIR)/NaClAMBullet.nmf

$(bullet_TGTS): $(bullet_SRCS)
	bullet/build.sh

#
# Lua
#
lua_SRC_DIR = lua
lua_TGT_DIR = lua/out/naclports/src/out/publish/lua/pnacl
lua_SRCS = $(lua_SRC_DIR)/build.sh
lua_TGTS = $(lua_TGT_DIR)/lua.pexe \
           $(lua_TGT_DIR)/scimark.lua \
           $(lua_TGT_DIR)/binarytrees.lua \
           $(lua_TGT_DIR)/luadata.tar \
           $(lua_TGT_DIR)/lua.nmf \
           $(lua_TGT_DIR)/hterm.concat.js

$(lua_TGTS): $(lua_SRCS)
	lua/build.sh

#
# Voronoi
#
voronoi_SRC_DIR = voronoi
voronoi_TGT_DIR = voronoi/pnacl/Release
voronoi_SRCS = $(voronoi_SRC_DIR)/voronoi.cc \
               $(voronoi_SRC_DIR)/Makefile \
               common/fps.h
voronoi_TGTS = $(voronoi_TGT_DIR)/voronoi.pexe \
               $(voronoi_TGT_DIR)/voronoi.nmf

$(voronoi_TGTS): $(voronoi_SRCS)
	$(MAKE) -C voronoi TOOLCHAIN=pnacl CONFIG=Release

#
# Life
#
life_SRC_DIR = life
life_TGT_DIR = life/pnacl/Release
life_SRCS = $(life_SRC_DIR)/life.c \
            $(life_SRC_DIR)/Makefile \
            common/fps.h
life_TGTS = $(life_TGT_DIR)/life.pexe \
            $(life_TGT_DIR)/life.nmf

$(life_TGTS): $(life_SRCS)
	$(MAKE) -C life TOOLCHAIN=pnacl CONFIG=Release

#
# SmoothLife
#
smoothlife_SRC_DIR = smoothlife
smoothlife_TGT_DIR = smoothlife/out
smoothlife_SRCS = $(smoothlife_SRC_DIR)/build.sh
smoothlife_TGTS = $(smoothlife_TGT_DIR)/smoothnacl.pexe \
                  $(smoothlife_TGT_DIR)/smoothnacl.nmf

$(smoothlife_TGTS): $(smoothlife_SRCS)
	smoothlife/build.sh


#
# Rotating Cube
#
cube_SRC_DIR = cube
cube_TGT_DIR = cube/pnacl/Release
cube_SRCS = $(cube_SRC_DIR)/cube.cc \
            $(cube_SRC_DIR)/matrix.cc \
            $(cube_SRC_DIR)/matrix.h \
            $(cube_SRC_DIR)/texture.cc \
            $(cube_SRC_DIR)/Makefile \
            common/fps.h
cube_TGTS = $(cube_TGT_DIR)/cube.pexe \
            $(cube_TGT_DIR)/cube.nmf

$(cube_TGTS): $(cube_SRCS)
	$(MAKE) -C cube TOOLCHAIN=pnacl CONFIG=Release

###############################################################################

SHELL = /bin/bash

OSHELPERS = python $(NACL_SDK_ROOT)/tools/oshelpers.py
GETOS := python $(NACL_SDK_ROOT)/tools/getos.py
WHICH := $(OSHELPERS) which

# Try the location of gsutil on the bots first...
BOT_GSUTIL = /b/build/scripts/slave/gsutil
ifneq (,$(wildcard $(BOT_GSUTIL)))
  GSUTIL = $(BOT_GSUTIL)
else
  GSUTIL = $(shell $(WHICH) gsutil)
  ifeq (,$(wildcard $(GSUTIL)))
    $(error Unable to find gstuil)
  endif
endif

#
# Define some variables for the given project.
#
# FOO_UPLOADS:
#     All files to upload for this project.
# FOO_CONTINUOUS_DIR:
#     URL of the continuous build directory for this
#     project and revision.
# FOO_CONTINUOUS_UPLOADS:
#     URLs of all files that will be uploaded for this
#     project and revision.
# FOO_PUBLISH_DIR:
#     URL of the publish directory for this project and revision.
#
# $1 = NAME (e.g. earth)
#
define PROJECT
  $(1)_UPLOADS = $$($(1)_TGTS) $$($(1)_EXTRA_UPLOADS)
  $(1)_CONTINUOUS_DIR = $(GS_URL_CONTINUOUS)/$(REVISION)/$(1)/
  $(1)_CONTINUOUS_UPLOADS = $$(addprefix $$($(1)_CONTINUOUS_DIR),$$(notdir $$($(1)_UPLOADS)))
  $(1)_PUBLISH_DIR = $(GS_URL_PUBLISH)/$(REVISION)/$(1)/

  all: $$($(1)_TGTS)

  .PHONY: $(1)
  $(1): $$($(1)_TGTS)
endef


#
# Define rules to upload the project files to the continuous builder directory
# on CDS.
#
# $1 = NAME (e.g. earth)
#
define UPLOAD_RULE
.PHONY: upload-$(1)
upload-$(1): revision-check
	@echo "Uploading $$(notdir $$($(1)_UPLOADS)) to $$($(1)_CONTINUOUS_DIR)"
	@$(GSUTIL) cp -q -a public-read $$($(1)_UPLOADS) $$($(1)_CONTINUOUS_DIR)

upload: upload-$(1)
endef

#
# Define rules to copy the project files from the continuous builder
# directory to the publish directory.
#
# $1 = NAME (e.g. earth)
#
define PUBLISH_RULE
.PHONY: publish-$(1)
publish-$(1): revision-check
	@echo "Testing that files to publish '$(1)' exist on CDS..."
	@$(GSUTIL) ls $$($(1)_CONTINUOUS_UPLOADS)
	@echo OK.
	@echo "About to publish revision $(REVISION) of '$(1)'..."
	@read -p "Continue? " -n 1 -r && \
	  echo && \
	  if [[ ! $$$${REPLY} =~ ^[Yy]$$$$ ]]; then \
	    exit 1; \
	  fi;
	@echo "Publishing..."
	@$(GSUTIL) cp -q -p $$($(1)_CONTINUOUS_UPLOADS) $$($(1)_PUBLISH_DIR)
	@echo "Done."

publish: publish-$(1)
endef


###############################################################################
# RULES

.PHONY: all
all:

$(foreach project,$(PROJECTS),$(eval $(call PROJECT,$(project))))

.PHONY: revision-check
revision-check:
ifeq (,$(REVISION))
	$(error Unknown revision number. Run with REVSION=<...>)
endif

.PHONY: upload
upload: all

.PHONY: publish
publish:

$(foreach project,$(PROJECTS),$(eval $(call UPLOAD_RULE,$(project))))
$(foreach project,$(PROJECTS),$(eval $(call PUBLISH_RULE,$(project))))
