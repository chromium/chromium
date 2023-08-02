// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/ios/display/gl_demo_screen.h"

#include "base/check.h"
#include "remoting/client/display/canvas.h"
#include "remoting/client/display/gl_math.h"

namespace remoting {

namespace {

const GLfloat square[] = {-1.0, -1.0, 1.0, -1.0, -1.0, 1.0, 1.0, 1.0};

const GLchar* fragmentShaderSource =
    "precision mediump float;"
    "void main() {"
    "  gl_FragColor = vec4(1.0, 1.0, 0.0, 1.0);"
    "} ";

const GLchar* vertexShaderSource =
    "precision mediump float;"
    "attribute vec4 a_position;"
    "void main() {"
    "  gl_Position = a_position;"
    "}";

const GLchar* a_position = "a_position";

}  // namespace

// This is a demo screen that can be added to the renderer to test the drawable
// integration. This will draw an expanding checkerboard pattern to the screen.
GlDemoScreen::GlDemoScreen() : weak_factory_(this)  {}

GlDemoScreen::~GlDemoScreen() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void GlDemoScreen::SetCanvas(base::WeakPtr<Canvas> canvas) {
  canvas_ = canvas;

  // Create and compile vertex shader.
  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
  glCompileShader(vertexShader);

  // Create and compile fragment shader.
  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
  glCompileShader(fragmentShader);

  // Create and link program.
  program_ = glCreateProgram();
  glAttachShader(program_, vertexShader);
  glAttachShader(program_, fragmentShader);
  glLinkProgram(program_);
}

int GlDemoScreen::GetZIndex() {
  return Drawable::DESKTOP + 1;
}

bool GlDemoScreen::Draw() {
  if (!canvas_) {
    return false;
  }

  // TODO(nicholss): width and height should be dynamic based on the canvas.
  int width = 640;
  int height = 1024;
  square_size_++;
  if (square_size_ > 300) {
    square_size_ = 1;
  }

  // Set the viewport.
  glViewport(0, 0, width, height);

  // Clear.
  glClearColor(0, 1, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);

  // Use program.
  glUseProgram(program_);

  int skip = 0;
  for (int i = 0; i < width; i += square_size_) {
    if (skip == square_size_) {
      skip = 0;
    } else {
      skip = square_size_;
    }
    for (int j = skip; j < height; j += square_size_ * 2) {
      glViewport(i, j, square_size_, square_size_);

      // Send geometry to vertex shader.
      GLuint aPosition = glGetAttribLocation(program_, a_position);

      glVertexAttribPointer(aPosition, 2, GL_FLOAT, GL_FALSE, 0, square);
      glEnableVertexAttribArray(aPosition);

      // Draw.
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
  }
  return false;
}

base::WeakPtr<Drawable> GlDemoScreen::GetWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_factory_.GetWeakPtr();
}

}  // namespace remoting
