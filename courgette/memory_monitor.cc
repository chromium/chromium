// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdio.h>
#include <map>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

bool inH = true;
struct H {
  H() { inH = false;  tick_ = 0; bw_ = 0; d_bw_ = d_tick_ = 0; m_bw_ = 0; mem_ = high_ = 0;}
  ~H() {
    inH = true;
    int i = 0;
    for (M::iterator p = m_.begin(); p != m_.end(); ++p, ++i) {
      size_t s = p->first;
      LOG(INFO) << base::StringPrintf("%3d %8u: %8u %8u %8u %8u", i, s,
             m_[s], c_[s], h_[s], h_[s] * s);
    }
    LOG(INFO) << "Peak " << fmt(high_);
  }

  std::string fmt(size_t s) {
    if (s > 1000000000) return base::StringPrintf("%.3gG", s/(1000000000.0));
    if (s > 1000000) return base::StringPrintf("%.3gM", s/(1000000.));
    if (s > 9999) return base::StringPrintf("%.3gk", s/(1000.));
    return base::NumberToString(s);
  }

  void tick(size_t w, char sign) {
    d_tick_ += 1;
    d_bw_ += w;
    const size_t T = 4*4*1024;
    const size_t M = 4*1024*1024;
    bool print = false;
    if (d_tick_ >= T) {
      tick_ += (d_tick_/T)*T;
      d_tick_ %= T;
      print = true;
    }
    if (d_bw_ >= M) {
      bw_ += (d_bw_/M) * M;
      d_bw_ %= M;
      print = true;
    }
    if (!print) return;
    std::string o;
    base::StringAppendF(&o, "%u:", tick_ + d_tick_);
    base::StringAppendF(&o, " (%c%s)", sign, fmt(w).c_str());
    size_t sum = 0;
    for (M::iterator p = c_.begin(); p != c_.end(); ++p) {
      size_t s = p->first;
      size_t n = p->second;
      if (n) {
        if (s*n >= 64*1024)
          if (n == 1)
            base::StringAppendF(&o, "  %s", fmt(s).c_str());
          else
            base::StringAppendF(&o, "  %s*%u", fmt(s).c_str(), n);
        sum += s*n;
        }
    }
    base::StringAppendF(&o, "  = %s", fmt(sum).c_str());
    LOG(INFO) << o;
    //printf("%s\n", o.c_str());
    if (sum > 200*1024*1024) {
      // __asm int 3;
      m_bw_ = sum;
    }
  }
  void add(size_t s, void *p) {
    if (!inH) {
      inH = true;
      mem_ += s; if (mem_ > high_) high_ = mem_;
      c_[s] += 1;
      m_[s] += 1;
      if (c_[s] > h_[s]) h_[s] = c_[s];
      allocs_[p] = s;
      inH = false;
      tick(s, '+');
    }
  }

  void sub(void *p) {
    if (!inH) {
      inH = true;
      size_t s = allocs_[p];
      if (s) {
        mem_ -= s;
        c_[s] -= 1;
        allocs_[p] = 0;
        tick(s, '-');
      }
      inH = false;
    }
  }

  typedef std::map<size_t, size_t> M;
  M m_;
  M c_;
  M h_;

  size_t bw_;
  size_t d_bw_;
  size_t tick_;
  size_t d_tick_;
  size_t m_bw_;
  size_t mem_;
  size_t high_;

  std::map<void*, size_t> allocs_;
} _H;

void* operator new(size_t s) {
  //printf("%u\n", s);
  void *p = malloc(s);
  _H.add(s, p);
  return p;
}

void operator delete(void *p) {
  _H.sub(p);
  free(p);
}
