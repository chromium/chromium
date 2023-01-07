/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {
  /* Use ppb_messaging to send "Hello World" to JavaScript. */
  printf("Hello World STDOUT.\n");

  /* Use ppb_console send "Hello World" to the JavaScript Console. */
  fprintf(stderr, "Hello World STDERR.\n");
  return 0;
}
