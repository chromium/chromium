/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef EXAMPLES_DEMO_NACL_IO_DEMO_HANDLERS_H_
#define EXAMPLES_DEMO_NACL_IO_DEMO_HANDLERS_H_

#include "ppapi/c/pp_var.h"

typedef int (*HandleFunc)(struct PP_Var params,
                          struct PP_Var* out_var,
                          const char** error);

int HandleFopen(struct PP_Var params, struct PP_Var* out, const char** error);
int HandleFwrite(struct PP_Var params, struct PP_Var* out, const char** error);
int HandleFread(struct PP_Var params, struct PP_Var* out, const char** error);
int HandleFseek(struct PP_Var params, struct PP_Var* out, const char** error);
int HandleFclose(struct PP_Var params, struct PP_Var* out, const char** error);
int HandleFflush(struct PP_Var params, struct PP_Var* out, const char** error);
int HandleStat(struct PP_Var params, struct PP_Var* out, const char** error);

int HandleOpendir(struct PP_Var params, struct PP_Var* out, const char** error);
int HandleReaddir(struct PP_Var params, struct PP_Var* out, const char** error);
int HandleClosedir(struct PP_Var params, struct PP_Var* out,
                   const char** error);

int HandleMkdir(struct PP_Var params, struct PP_Var* out, const char** error);
int HandleRmdir(struct PP_Var params, struct PP_Var* out, const char** error);
int HandleChdir(struct PP_Var params, struct PP_Var* out, const char** error);
int HandleGetcwd(struct PP_Var params, struct PP_Var* out, const char** error);

int HandleGetaddrinfo(struct PP_Var params, struct PP_Var* out,
                      const char** error);
int HandleGethostbyname(struct PP_Var params, struct PP_Var* out,
                        const char** error);
int HandleConnect(struct PP_Var params, struct PP_Var* out, const char** error);
int HandleSend(struct PP_Var params, struct PP_Var* out, const char** error);
int HandleRecv(struct PP_Var params, struct PP_Var* out, const char** error);
int HandleClose(struct PP_Var params, struct PP_Var* out, const char** error);

#endif  // EXAMPLES_DEMO_NACL_IO_DEMO_HANDLERS_H_
