// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/tests/lpm/gl_lpm_shader_to_string.h"

#include <string>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/text_format.h"

class LpmShaderTest
    : public ::testing::TestWithParam<std::pair<std::string, std::string>> {};

TEST_P(LpmShaderTest, CheckTranslation) {
  const std::pair<std::string, std::string>& param = GetParam();
  fuzzing::Shader shader;
  EXPECT_TRUE(
      google::protobuf::TextFormat::ParseFromString(param.first, &shader));
  ASSERT_EQ(gl_lpm_fuzzer::GetShader(shader), param.second);
}

static const char* kProtoAssignment = R"(
functions {
  function_name: MAIN
  block {
    statements {
      assignment {
        lvalue { var: VAR_0 }
        rvalue {}
      }
    }
  }
  return_stmt {}
  type: VOID_TYPE
})";

static const char* kShaderAssignment = R"(#version 300 es
void main() {
var0 = 1;
return 1;
})";

static const char* kProtoLoop = R"(functions {
  function_name: MAIN
  block {
    statements {
      while_stmt {
        cond {
          binary_op {
            op: PLUS
            left {
              cons {
                int32: 0
              }
            }
            right {
            }
          }
        }
        body {
        }
      }
    }
  }
  return_stmt {
  }
  type: VOID_TYPE
})";

static const char* kShaderLoop = R"(#version 300 es
void main() {
while ((0 + 1)) {
}
return 1;
})";

static const char* kProtoTwoFunctions = R"(functions {
  function_name: MAIN
  block {
    statements {
      while_stmt {
        cond {
          var: VAR_0
        }
        body {
        }
      }
    }
  }
  return_stmt {
  }
  type: VOID_TYPE
}
functions {
  function_name: MAIN
  block {
    statements {
    }
  }
  return_stmt {
  }
  type: VOID_TYPE
})";

static const char* kShaderTwoFunctions = R"(#version 300 es
void main() {
while (0) {
}
return 1;
}
void main() {
return 1;
})";

static const char* kProtoDeclareFloat = R"(functions {
  function_name: NAME_2
  block {
  }
  return_stmt {
  }
  type: VOID_TYPE
}
functions {
  function_name: NAME_1
  block {
    statements {
      declare {
        type: VOID_TYPE
        var: VAR_2
      }
    }
  }
  return_stmt {
  }
  type: VOID_TYPE
})";

static const char* kShaderDeclareFloat = R"(#version 300 es
void f2() {
return 1;
}
void f1() {
float var2;
return 1;
})";

static const char* kProtoIfElse = R"(functions {
  function_name: NAME_2
  block {
  }
  return_stmt {
  }
  type: VOID_TYPE
}
functions {
  function_name: MAIN
  block {
    statements {
      ifelse {
        cond {
          cons {
            int32: 0
          }
        }
        if_body {
        }
        else_body {
        }
      }
    }
  }
  return_stmt {
    cons {
      int32: 0
    }
  }
  type: VOID_TYPE
})";

static const char* kShaderIfElse = R"(#version 300 es
void f2() {
return 1;
}
void main() {
if (0) {
} else {
}
return 0;
})";

static const char* kProtoConstInt32 = R"(functions {
  return_stmt {
    cons {
      int32: 117506048
    }
  }
})";

static const char* kShaderConstInt32 = R"(#version 300 es
void main() {
return 117506048;
})";

// Uniform only applies at global scope
static const char* kProtoNoLocalUniform = R"(functions {
  block {
    statements {
      while_stmt {
        body {
          statements {
            declare {
              qualifier: UNIFORM_QUALIFIER
            }
          }
        }
      }
    }
  }
})";

static const char* kShaderNoLocalUniform = R"(#version 300 es
void main() {
while (1) {
float var0;
}
return;
})";

INSTANTIATE_TEST_SUITE_P(
    LpmFuzzer,
    LpmShaderTest,
    ::testing::Values(std::make_pair(kProtoAssignment, kShaderAssignment),
                      std::make_pair(kProtoLoop, kShaderLoop),
                      std::make_pair(kProtoTwoFunctions, kShaderTwoFunctions),
                      std::make_pair(kProtoDeclareFloat, kShaderDeclareFloat),
                      std::make_pair(kProtoIfElse, kShaderIfElse),
                      std::make_pair(kProtoConstInt32, kShaderConstInt32),
                      std::make_pair(kProtoNoLocalUniform,
                                     kShaderNoLocalUniform)));
