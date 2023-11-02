// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/devfs/tty_node.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>

#include "nacl_io/filesystem.h"
#include "nacl_io/ioctl.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/log.h"
#include "nacl_io/pepper_interface.h"
#include "sdk_util/auto_lock.h"

#define CHECK_LFLAG(TERMIOS, FLAG) (TERMIOS.c_lflag& FLAG)

#define IS_ECHO CHECK_LFLAG(termios_, ECHO)
#define IS_ECHOE CHECK_LFLAG(termios_, ECHOE)
#define IS_ECHONL CHECK_LFLAG(termios_, ECHONL)
#define IS_ECHOCTL CHECK_LFLAG(termios_, ECHOCTL)
#define IS_ICANON CHECK_LFLAG(termios_, ICANON)

#define DEFAULT_TTY_COLS 80
#define DEFAULT_TTY_ROWS 30

namespace nacl_io {

TtyNode::TtyNode(Filesystem* filesystem)
    : CharNode(filesystem),
      emitter_(new EventEmitter),
      rows_(DEFAULT_TTY_ROWS),
      cols_(DEFAULT_TTY_COLS) {
  output_handler_.handler = NULL;
  InitTermios();

  // Output will never block
  emitter_->RaiseEvents_Locked(POLLOUT);
}

void TtyNode::InitTermios() {
  // Some sane values that produce good result.
  termios_.c_iflag = ICRNL | IXON | IXOFF;
#ifdef IUTF8
  termios_.c_iflag |= IUTF8;
#endif
  termios_.c_oflag = OPOST | ONLCR;
  termios_.c_cflag = CREAD | 077;
  termios_.c_lflag =
      ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN;
#if !defined(__BIONIC__) && !(defined(__GLIBC__) && defined(__arm__))
  termios_.c_ispeed = B38400;
  termios_.c_ospeed = B38400;
#endif
  termios_.c_cc[VINTR] = 3;
  termios_.c_cc[VQUIT] = 28;
  termios_.c_cc[VERASE] = 127;
  termios_.c_cc[VKILL] = 21;
  termios_.c_cc[VEOF] = 4;
  termios_.c_cc[VTIME] = 0;
  termios_.c_cc[VMIN] = 1;
#if defined(VSWTC) /* Not defined on on Mac */
  termios_.c_cc[VSWTC] = 0;
#endif
  termios_.c_cc[VSTART] = 17;
  termios_.c_cc[VSTOP] = 19;
  termios_.c_cc[VSUSP] = 26;
  termios_.c_cc[VEOL] = 0;
  termios_.c_cc[VREPRINT] = 18;
  termios_.c_cc[VDISCARD] = 15;
  termios_.c_cc[VWERASE] = 23;
  termios_.c_cc[VLNEXT] = 22;
  termios_.c_cc[VEOL2] = 0;
}

EventEmitter* TtyNode::GetEventEmitter() {
  return emitter_.get();
}

Error TtyNode::Write(const HandleAttr& attr,
                     const void* buf,
                     size_t count,
                     int* out_bytes) {
  AUTO_LOCK(output_lock_);
  *out_bytes = 0;

  // No handler registered.
  if (output_handler_.handler == NULL) {
    // No error here; many of the tests trigger this message.
    LOG_TRACE("No output handler registered.");
    return EIO;
  }

  int rtn = output_handler_.handler(
      static_cast<const char*>(buf), count, output_handler_.user_data);

  // Negative return value means an error occurred and the return
  // value is a negated errno value.
  if (rtn < 0)
    return -rtn;

  *out_bytes = rtn;
  return 0;
}

Error TtyNode::Read(const HandleAttr& attr,
                    void* buf,
                    size_t count,
                    int* out_bytes) {
  EventListenerLock wait(GetEventEmitter());
  *out_bytes = 0;

  // If interrupted, return
  int ms = attr.IsBlocking() ? -1 : 0;
  Error err = wait.WaitOnEvent(POLLIN, ms);
  if (err == ETIMEDOUT)
    err = EWOULDBLOCK;
  if (err != 0)
    return err;

  size_t bytes_to_copy = std::min(count, input_buffer_.size());
  if (IS_ICANON) {
    // Only read up to (and including) the first newline
    std::deque<char>::iterator nl =
        std::find(input_buffer_.begin(), input_buffer_.end(), '\n');

    if (nl != input_buffer_.end()) {
      // We found a newline in the buffer, adjust bytes_to_copy accordingly
      size_t line_len = static_cast<size_t>(nl - input_buffer_.begin()) + 1;
      bytes_to_copy = std::min(bytes_to_copy, line_len);
    }
  }

  // Copies data from the input buffer into buf.
  std::copy(input_buffer_.begin(),
            input_buffer_.begin() + bytes_to_copy,
            static_cast<char*>(buf));
  *out_bytes = bytes_to_copy;
  input_buffer_.erase(input_buffer_.begin(),
                      input_buffer_.begin() + bytes_to_copy);

  // mark input as no longer readable if we consumed
  // the entire buffer or, in the case of buffered input,
  // we consumed the final \n char.
  bool avail;
  if (IS_ICANON)
    avail = std::find(input_buffer_.begin(), input_buffer_.end(), '\n') !=
            input_buffer_.end();
  else
    avail = input_buffer_.size() > 0;

  if (!avail)
    emitter_->ClearEvents_Locked(POLLIN);

  return 0;
}

Error TtyNode::Echo(const char* string, int count) {
  int wrote;
  HandleAttr data;
  Error error = Write(data, string, count, &wrote);
  if (error != 0 || wrote != count) {
    // TODO(sbc): Do something more useful in response to a
    // failure to echo.
    return error;
  }

  return 0;
}

Error TtyNode::ProcessInput(PP_Var message) {
  if (message.type != PP_VARTYPE_STRING) {
    LOG_ERROR("Expected VarString but got %d.", message.type);
    return EINVAL;
  }

  PepperInterface* ppapi = filesystem_->ppapi();
  if (!ppapi) {
    LOG_ERROR("ppapi is NULL.");
    return EINVAL;
  }

  VarInterface* var_iface = ppapi->GetVarInterface();
  if (!var_iface) {
    LOG_ERROR("Got NULL interface: Var");
    return EINVAL;
  }

  uint32_t num_bytes;
  const char* buffer = var_iface->VarToUtf8(message, &num_bytes);
  Error error = ProcessInput(buffer, num_bytes);
  return error;
}

Error TtyNode::ProcessInput(const char* buffer, size_t num_bytes) {
  AUTO_LOCK(emitter_->GetLock())

  for (size_t i = 0; i < num_bytes; i++) {
    char c = buffer[i];
    // Transform characters according to input flags.
    if (c == '\r') {
      if (termios_.c_iflag & IGNCR)
        continue;
      if (termios_.c_iflag & ICRNL)
        c = '\n';
    } else if (c == '\n') {
      if (termios_.c_iflag & INLCR)
        c = '\r';
    }

    bool skip = false;

    // ICANON mode means we wait for a newline before making the
    // file readable.
    if (IS_ICANON) {
      if (IS_ECHOE && c == termios_.c_cc[VERASE]) {
        // Remove previous character in the line if any.
        if (!input_buffer_.empty()) {
          char char_to_delete = input_buffer_.back();
          if (char_to_delete != '\n') {
            input_buffer_.pop_back();
            if (IS_ECHO)
              Echo("\b \b", 3);

            // When ECHOCTL is set the echo buffer contains an extra
            // char for each control char.
            if (IS_ECHOCTL && iscntrl(char_to_delete))
              Echo("\b \b", 3);
          }
        }
        continue;
      } else if (IS_ECHO || (IS_ECHONL && c == '\n')) {
        if (c == termios_.c_cc[VEOF]) {
          // VEOF sequence is not echoed, nor is it sent as
          // input.
          skip = true;
        } else if (c != '\n' && iscntrl(c) && IS_ECHOCTL) {
          // In ECHOCTL mode a control char C is echoed  as '^'
          // followed by the ascii char which at C + 0x40.
          char visible_char = c + 0x40;
          Echo("^", 1);
          Echo(&visible_char, 1);
        } else {
          Echo(&c, 1);
        }
      }
    }

    if (!skip)
      input_buffer_.push_back(c);

    if (c == '\n' || c == termios_.c_cc[VEOF] || !IS_ICANON)
      emitter_->RaiseEvents_Locked(POLLIN);
  }

  return 0;
}

Error TtyNode::VIoctl(int request, va_list args) {
  /*
   * Casts required for some of these case statements in order to silence
   * compiler warning when built with darwin headers.
   */
  switch (request) {
    case TIOCNACLOUTPUT: {
      struct tioc_nacl_output* arg = va_arg(args, struct tioc_nacl_output*);
      AUTO_LOCK(output_lock_);
      if (arg == NULL) {
        output_handler_.handler = NULL;
        return 0;
      }
      if (output_handler_.handler != NULL) {
        LOG_ERROR("Output handler already set.");
        return EALREADY;
      }
      output_handler_ = *arg;
      return 0;
    }
    case NACL_IOC_HANDLEMESSAGE: {
      struct PP_Var* message = va_arg(args, struct PP_Var*);
      return ProcessInput(*message);
    }
    case (unsigned int)TIOCSWINSZ: {
      struct winsize* size = va_arg(args, struct winsize*);
      {
        AUTO_LOCK(node_lock_);
        if (rows_ == size->ws_row && cols_ == size->ws_col)
          return 0;
        rows_ = size->ws_row;
        cols_ = size->ws_col;
      }
      ki_kill(getpid(), SIGWINCH);
      {
        // Wake up any thread waiting on Read with POLLERR then immediate
        // clear it to signal EINTR.
        AUTO_LOCK(emitter_->GetLock())
        emitter_->RaiseEvents_Locked(POLLERR);
        emitter_->ClearEvents_Locked(POLLERR);
      }
      return 0;
    }
    case (unsigned int)TIOCGWINSZ: {
      struct winsize* size = va_arg(args, struct winsize*);
      size->ws_row = rows_;
      size->ws_col = cols_;
      return 0;
    }
    default: {
      LOG_ERROR("TtyNode:VIoctl: Unknown request: %#x", request);
    }
  }

  return EINVAL;
}

Error TtyNode::Tcgetattr(struct termios* termios_p) {
  AUTO_LOCK(node_lock_);
  *termios_p = termios_;
  return 0;
}

Error TtyNode::Tcsetattr(int optional_actions,
                         const struct termios* termios_p) {
  AUTO_LOCK(node_lock_);
  termios_ = *termios_p;
  return 0;
}

}  // namespace nacl_io
