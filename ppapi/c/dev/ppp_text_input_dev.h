/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppp_text_input_dev.idl modified Thu Mar 28 10:55:30 2013. */

#ifndef PPAPI_C_DEV_PPP_TEXT_INPUT_DEV_H_
#define PPAPI_C_DEV_PPP_TEXT_INPUT_DEV_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

#define PPP_TEXTINPUT_DEV_INTERFACE_0_1 "PPP_TextInput(Dev);0.1"
#define PPP_TEXTINPUT_DEV_INTERFACE PPP_TEXTINPUT_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPP_TextInput_Dev</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * <code>PPP_TextInput_Dev</code> is a set of function pointers that the
 * plugin has to implement to provide hints for text input system (IME).
 */
struct PPP_TextInput_Dev_0_1 {
  /**
   * Requests the plugin to send back the text around the current caret or
   * selection by <code>PPB_TextInput_Dev::UpdateSurroundingText</code>.
   * It is recommended to include the <code>desired_number_of_characters</code>
   * characters before and after the selection, but not mandatory.
   */
  void (*RequestSurroundingText)(PP_Instance instance,
                                 uint32_t desired_number_of_characters);
};

typedef struct PPP_TextInput_Dev_0_1 PPP_TextInput_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPP_TEXT_INPUT_DEV_H_ */

