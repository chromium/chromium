// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_IDS_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_IDS_AUTOGEN_H_

#define RASTER_COMMAND_LIST(OP)                          \
  OP(DeleteTexturesImmediate)                  /* 256 */ \
  OP(Finish)                                   /* 257 */ \
  OP(Flush)                                    /* 258 */ \
  OP(GetError)                                 /* 259 */ \
  OP(GetIntegerv)                              /* 260 */ \
  OP(GenQueriesEXTImmediate)                   /* 261 */ \
  OP(DeleteQueriesEXTImmediate)                /* 262 */ \
  OP(BeginQueryEXT)                            /* 263 */ \
  OP(EndQueryEXT)                              /* 264 */ \
  OP(LoseContextCHROMIUM)                      /* 265 */ \
  OP(InsertFenceSyncCHROMIUM)                  /* 266 */ \
  OP(WaitSyncTokenCHROMIUM)                    /* 267 */ \
  OP(UnpremultiplyAndDitherCopyCHROMIUM)       /* 268 */ \
  OP(BeginRasterCHROMIUMImmediate)             /* 269 */ \
  OP(RasterCHROMIUM)                           /* 270 */ \
  OP(EndRasterCHROMIUM)                        /* 271 */ \
  OP(CreateTransferCacheEntryINTERNAL)         /* 272 */ \
  OP(DeleteTransferCacheEntryINTERNAL)         /* 273 */ \
  OP(UnlockTransferCacheEntryINTERNAL)         /* 274 */ \
  OP(CreateTexture)                            /* 275 */ \
  OP(SetColorSpaceMetadata)                    /* 276 */ \
  OP(ProduceTextureDirectImmediate)            /* 277 */ \
  OP(CreateAndConsumeTextureINTERNALImmediate) /* 278 */ \
  OP(TexParameteri)                            /* 279 */ \
  OP(BindTexImage2DCHROMIUM)                   /* 280 */ \
  OP(ReleaseTexImage2DCHROMIUM)                /* 281 */ \
  OP(TexStorage2D)                             /* 282 */ \
  OP(CopySubTexture)                           /* 283 */ \
  OP(TraceBeginCHROMIUM)                       /* 284 */ \
  OP(TraceEndCHROMIUM)                         /* 285 */ \
  OP(SetActiveURLCHROMIUM)                     /* 286 */

enum CommandId {
  kOneBeforeStartPoint =
      cmd::kLastCommonId,  // All Raster commands start after this.
#define RASTER_CMD_OP(name) k##name,
  RASTER_COMMAND_LIST(RASTER_CMD_OP)
#undef RASTER_CMD_OP
      kNumCommands,
  kFirstRasterCommand = kOneBeforeStartPoint + 1
};

#endif  // GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_IDS_AUTOGEN_H_
