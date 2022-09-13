// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/tests/lpm/gl_lpm_shader_to_string.h"

#include <ostream>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace gl_lpm_fuzzer {

std::string GetFunctionName(const fuzzing::FunctionName& function_name) {
  if (function_name == fuzzing::MAIN) {
    return "main";
  }
  return "f" + base::NumberToString(function_name);
}

std::string GetType(const fuzzing::Type& type, bool void_ok) {
  switch (type) {
    case fuzzing::VOID_TYPE: {
      // Avoid void in variable declarations
      if (!void_ok) {
        return "float";
      }
      return "void";
    }
    case fuzzing::INT: {
      return "int";
    }
    case fuzzing::BOOL: {
      return "bool";
    }
    case fuzzing::UINT: {
      return "uint";
    }
    case fuzzing::FLOAT: {
      return "float";
    }
    case fuzzing::DOUBLE: {
      return "double";
    }
    case fuzzing::VEC2: {
      return "vec2";
    }
    case fuzzing::VEC3: {
      return "vec3";
    }
    case fuzzing::VEC4: {
      return "vec4";
    }
    case fuzzing::BVEC2: {
      return "bvec2";
    }
    case fuzzing::BVEC3: {
      return "bvec3";
    }
    case fuzzing::BVEC4: {
      return "bvec4";
    }
    case fuzzing::IVEC2: {
      return "ivec2";
    }
    case fuzzing::IVEC3: {
      return "ivec3";
    }
    case fuzzing::IVEC4: {
      return "ivec4";
    }
    case fuzzing::UVEC2: {
      return "uvec2";
    }
    case fuzzing::UVEC3: {
      return "uvec3";
    }
    case fuzzing::UVEC4: {
      return "uvec4";
    }
    case fuzzing::MAT2: {
      return "mat2";
    }
    case fuzzing::MAT3: {
      return "mat3";
    }
    case fuzzing::MAT4: {
      return "mat4";
    }
    case fuzzing::MAT2X2: {
      return "mat2x2";
    }
    case fuzzing::MAT2X3: {
      return "mat2x3";
    }
    case fuzzing::MAT2X4: {
      return "mat2x4";
    }
    case fuzzing::MAT3X2: {
      return "mat3x2";
    }
    case fuzzing::MAT3X3: {
      return "mat3x3";
    }
    case fuzzing::MAT3X4: {
      return "mat3x4";
    }
    case fuzzing::MAT4X2: {
      return "mat4x2";
    }
    case fuzzing::MAT4X3: {
      return "mat4x3";
    }
    case fuzzing::MAT4X4: {
      return "mat4x4";
    }
  }
  CHECK(false);
  return "";
}

std::ostream& operator<<(std::ostream& os, const fuzzing::Statement& statement);
std::ostream& operator<<(std::ostream& os, const fuzzing::Rvalue& rvalue);

std::ostream& operator<<(std::ostream& os, const fuzzing::Block& block) {
  for (const fuzzing::Statement& statement : block.statements()) {
    os << statement;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const fuzzing::IfElse& ifelse) {
  return os << "if (" << ifelse.cond() << ") {\n"
            << ifelse.if_body() << "} else {\n"
            << ifelse.else_body() << "}\n";
}

std::ostream& operator<<(std::ostream& os, const fuzzing::Vec2& vec2) {
  return os << "vec2(" << vec2.first() << ", " << vec2.second() << ")";
}

std::ostream& operator<<(std::ostream& os, const fuzzing::Vec3& vec3) {
  return os << "vec3(" << vec3.first() << ", " << vec3.second() << ", "
            << vec3.third() << ")";
}

std::ostream& operator<<(std::ostream& os, const fuzzing::Vec4& vec4) {
  return os << "vec4(" << vec4.first() << ", " << vec4.second() << ", "
            << vec4.third() << ", " << vec4.fourth() << ")";
}

std::string BoolToString(bool boolean) {
  if (!boolean) {
    return "false";
  }
  return "true";
}

std::ostream& operator<<(std::ostream& os, const fuzzing::Const& cons) {
  switch (cons.value_case()) {
    case fuzzing::Const::VALUE_NOT_SET: {
      os << "1";
      break;
    }
    case fuzzing::Const::kInt32: {
      os << base::NumberToString(cons.int32());
      break;
    }
    case fuzzing::Const::kBoolean: {
      os << BoolToString(cons.boolean());
      break;
    }
    case fuzzing::Const::kUint: {
      os << base::NumberToString(cons.uint()) << "u";
      break;
    }
    case fuzzing::Const::kFloatConst: {
      os << base::NumberToString(cons.float_const());
      break;
    }
    case fuzzing::Const::kDoubleConst: {
      os << base::NumberToString(cons.double_const());
      break;
    }
    case fuzzing::Const::kVec2: {
      os << cons.vec2();
      break;
    }
    case fuzzing::Const::kVec3: {
      os << cons.vec3();
      break;
    }
    case fuzzing::Const::kVec4: {
      os << cons.vec4();
      break;
    }
    case fuzzing::Const::kBvec2: {
      os << "bvec2(" << BoolToString(cons.bvec2().first()) << ", "
         << BoolToString(cons.bvec2().second()) << ")";
      break;
    }
    case fuzzing::Const::kBvec3: {
      os << "bvec3(" << BoolToString(cons.bvec3().first()) << ", "
         << BoolToString(cons.bvec3().second()) << ", "
         << BoolToString(cons.bvec3().third()) << ")";
      break;
    }
    case fuzzing::Const::kBvec4: {
      os << "bvec4(" << BoolToString(cons.bvec4().first()) << ", "
         << BoolToString(cons.bvec4().second()) << ", "
         << BoolToString(cons.bvec4().third()) << ", "
         << BoolToString(cons.bvec4().fourth()) << ")";
      break;
    }
    case fuzzing::Const::kIvec2: {
      os << "ivec2(" << base::NumberToString(cons.ivec2().first()) << ", "
         << base::NumberToString(cons.ivec2().second()) << ")";
      break;
    }
    case fuzzing::Const::kIvec3: {
      os << "ivec3(" << base::NumberToString(cons.ivec3().first()) << ", "
         << base::NumberToString(cons.ivec3().second()) << ", "
         << base::NumberToString(cons.ivec3().third()) << ")";
      break;
    }
    case fuzzing::Const::kIvec4: {
      os << "ivec4(" << base::NumberToString(cons.ivec4().first()) << ", "
         << base::NumberToString(cons.ivec4().second()) << ", "
         << base::NumberToString(cons.ivec4().third()) << ", "
         << base::NumberToString(cons.ivec4().fourth()) << ")";
      break;
    }
    case fuzzing::Const::kUvec2: {
      os << "uvec2(" << base::NumberToString(cons.uvec2().first()) << "u, "
         << base::NumberToString(cons.uvec2().second()) << "u)";
      break;
    }
    case fuzzing::Const::kUvec3: {
      os << "uvec3(" << base::NumberToString(cons.uvec3().first()) << "u, "
         << base::NumberToString(cons.uvec3().second()) << "u, "
         << base::NumberToString(cons.uvec3().third()) << "u)";
      break;
    }
    case fuzzing::Const::kUvec4: {
      os << "uvec4(" << base::NumberToString(cons.uvec4().first()) << "u, "
         << base::NumberToString(cons.uvec4().second()) << "u, "
         << base::NumberToString(cons.uvec4().third()) << "u, "
         << base::NumberToString(cons.uvec4().fourth()) << "u)";
      break;
    }
    case fuzzing::Const::kMat2: {
      os << "mat2(" << cons.mat2().first() << ", " << cons.mat2().second()
         << ")";
      break;
    }
    case fuzzing::Const::kMat3: {
      os << "mat3(" << cons.mat3().first() << ", " << cons.mat3().second()
         << ", " << cons.mat3().third() << ")";
      break;
    }
    case fuzzing::Const::kMat4: {
      os << "mat4(" << cons.mat4().first() << ", " << cons.mat4().second()
         << ", " << cons.mat4().third() << ", " << cons.mat4().fourth() << ")";
      break;
    }
    case fuzzing::Const::kMat2X2: {
      os << "mat2x2(" << cons.mat2x2().first() << ", " << cons.mat2().second()
         << ")";
      break;
    }
    case fuzzing::Const::kMat2X3: {
      os << "mat2x3(" << cons.mat2x3().first() << ", " << cons.mat2x3().second()
         << ")";
      break;
    }
    case fuzzing::Const::kMat2X4: {
      os << "mat2x4(" << cons.mat2x4().first() << ", " << cons.mat2x4().second()
         << ")";
      break;
    }
    case fuzzing::Const::kMat3X2: {
      os << "mat3x2(" << cons.mat3x2().first() << ", " << cons.mat3x2().second()
         << ", " << cons.mat3x2().third() << ")";
      break;
    }
    case fuzzing::Const::kMat3X3: {
      os << "mat3x3(" << cons.mat3x3().first() << ", " << cons.mat3x3().second()
         << ", " << cons.mat3x3().third() << ")";
      break;
    }
    case fuzzing::Const::kMat3X4: {
      os << "mat3x4(" << cons.mat3x4().first() << ", " << cons.mat3x4().second()
         << ", " << cons.mat3x4().third() << ")";
      break;
    }
    case fuzzing::Const::kMat4X2: {
      os << "mat4x2(" << cons.mat4x2().first() << ", " << cons.mat4x2().second()
         << ", " << cons.mat4x2().third() << ", " << cons.mat4x2().fourth()
         << ")";
      break;
    }
    case fuzzing::Const::kMat4X3: {
      os << "mat4x3(" << cons.mat4x3().first() << ", " << cons.mat4x3().second()
         << ", " << cons.mat4x3().third() << ", " << cons.mat4x3().fourth()
         << ")";
      break;
    }
    case fuzzing::Const::kMat4X4: {
      os << "mat4x4(" << cons.mat4x4().first() << ", " << cons.mat4x4().second()
         << ", " << cons.mat4x4().third() << ", " << cons.mat4x4().fourth()
         << ")";
      break;
    }
  }
  return os;
}

std::string GetBinaryOp(const fuzzing::BinaryOp::Op op) {
  switch (op) {
    case fuzzing::BinaryOp::PLUS:
      return "+";
    case fuzzing::BinaryOp::MINUS:
      return "-";
    case fuzzing::BinaryOp::MUL:
      return "*";
    case fuzzing::BinaryOp::DIV:
      return "/";
    case fuzzing::BinaryOp::MOD:
      return "%";
    case fuzzing::BinaryOp::XOR:
      return "^";
    case fuzzing::BinaryOp::AND:
      return "&";
    case fuzzing::BinaryOp::OR:
      return "|";
    case fuzzing::BinaryOp::EQ:
      return "==";
    case fuzzing::BinaryOp::NE:
      return "!=";
    case fuzzing::BinaryOp::LE:
      return "<=";
    case fuzzing::BinaryOp::GE:
      return ">=";
    case fuzzing::BinaryOp::LT:
      return "<";
    case fuzzing::BinaryOp::GT:
      return ">";
    case fuzzing::BinaryOp::SHL:
      return "<<";
    case fuzzing::BinaryOp::SHR:
      return ">>";
    case fuzzing::BinaryOp::LOGICAL_AND:
      return "&&";
    case fuzzing::BinaryOp::LOGICAL_OR:
      return "||";
    default:
      DCHECK(false);
  }
  return "";
}

std::string GetUnaryOp(const fuzzing::UnaryOp::Op op) {
  switch (op) {
    case fuzzing::UnaryOp::PLUS:
      return "+";
    case fuzzing::UnaryOp::MINUS:
      return "-";
    case fuzzing::UnaryOp::TILDE:
      return "~";
    case fuzzing::UnaryOp::NOT:
      return "!";
  }
}

std::ostream& operator<<(std::ostream& os, const fuzzing::BinaryOp& binary_op) {
  return os << "(" << binary_op.left() << " " << GetBinaryOp(binary_op.op())
            << " " << binary_op.right() << ")";
}

std::ostream& operator<<(std::ostream& os, const fuzzing::UnaryOp& unary_op) {
  return os << GetUnaryOp(unary_op.op()) << unary_op.rvalue();
}

std::ostream& operator<<(std::ostream& os, const fuzzing::Rvalue& rvalue) {
  switch (rvalue.rvalue_case()) {
    case fuzzing::Rvalue::kVar: {
      os << rvalue.var();
      break;
    }
    case fuzzing::Rvalue::kCons: {
      os << rvalue.cons();
      break;
    }
    case fuzzing::Rvalue::kBinaryOp: {
      os << rvalue.binary_op();
      break;
    }
    case fuzzing::Rvalue::kUnaryOp: {
      os << rvalue.unary_op();
      break;
    }
    case fuzzing::Rvalue::RVALUE_NOT_SET: {
      os << "1";
      break;
    }
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const fuzzing::While& while_stmt) {
  return os << "while (" << while_stmt.cond() << ") {\n"
            << while_stmt.body() << "}\n";
}

std::string GetVar(const fuzzing::Var& var) {
  return "var" + base::NumberToString(var);
}

std::ostream& operator<<(std::ostream& os, const fuzzing::Lvalue& lvalue) {
  return os << GetVar(lvalue.var());
}

std::ostream& operator<<(std::ostream& os,
                         const fuzzing::Assignment& assignment) {
  return os << assignment.lvalue() << " = " << assignment.rvalue() << ";\n";
}

bool TypeIsBool(const fuzzing::Type& type) {
  switch (type) {
    case fuzzing::BOOL:
    case fuzzing::BVEC2:
    case fuzzing::BVEC3:
    case fuzzing::BVEC4:
      return true;
    case fuzzing::VOID_TYPE:
    case fuzzing::INT:
    case fuzzing::UINT:
    case fuzzing::FLOAT:
    case fuzzing::DOUBLE:
    case fuzzing::VEC2:
    case fuzzing::VEC3:
    case fuzzing::VEC4:
    case fuzzing::IVEC2:
    case fuzzing::IVEC3:
    case fuzzing::IVEC4:
    case fuzzing::UVEC2:
    case fuzzing::UVEC3:
    case fuzzing::UVEC4:
    case fuzzing::MAT2:
    case fuzzing::MAT3:
    case fuzzing::MAT4:
    case fuzzing::MAT2X2:
    case fuzzing::MAT2X3:
    case fuzzing::MAT2X4:
    case fuzzing::MAT3X2:
    case fuzzing::MAT3X3:
    case fuzzing::MAT3X4:
    case fuzzing::MAT4X2:
    case fuzzing::MAT4X3:
    case fuzzing::MAT4X4:
      return false;
  }
}

void GetDeclaration(std::ostream& os,
                    const fuzzing::Declare& declare,
                    bool is_global) {
  switch (declare.qualifier()) {
    case fuzzing::Declare::NO_QUALIFIER: {
      break;
    }
    case fuzzing::Declare::CONST_QUALIFIER: {
      os << "const ";
      break;
    }
    case fuzzing::Declare::IN_QUALIFIER: {
      // 'in' cannot be bool
      if (TypeIsBool(declare.type())) {
        break;
      }
      os << "in ";
      break;
    }
    case fuzzing::Declare::OUT_QUALIFIER: {
      // 'out' cannot be bool
      if (TypeIsBool(declare.type())) {
        break;
      }
      os << "out ";
      break;
    }
    case fuzzing::Declare::UNIFORM_QUALIFIER: {
      if (!is_global) {
        break;
      }
      os << "uniform ";
      break;
    }
  }
  os << GetType(declare.type(), /*void_ok=*/false) << " "
     << GetVar(declare.var());
  // Const must be initialized, other types cannot be
  if (declare.qualifier() == fuzzing::Declare::CONST_QUALIFIER ||
      (declare.qualifier() == fuzzing::Declare::NO_QUALIFIER &&
       declare.has_rvalue())) {
    os << " = " << declare.rvalue();
  }
  os << ";\n";
}

std::ostream& operator<<(std::ostream& os,
                         const fuzzing::Statement& statement) {
  switch (statement.statement_case()) {
    case fuzzing::Statement::STATEMENT_NOT_SET: {
      break;
    }
    case fuzzing::Statement::kAssignment: {
      os << statement.assignment();
      break;
    }
    case fuzzing::Statement::kIfelse: {
      os << statement.ifelse();
      break;
    }
    case fuzzing::Statement::kWhileStmt: {
      os << statement.while_stmt();
      break;
    }
    case fuzzing::Statement::kReturnStmt: {
      os << "return " << statement.return_stmt() << ";\n";
      break;
    }
    case fuzzing::Statement::kDeclare: {
      GetDeclaration(os, statement.declare(), /*is_global=*/false);
      break;
    }
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const fuzzing::Function& function) {
  os << GetType(function.type(), /*void_ok=*/true) << " "
     << GetFunctionName(function.function_name()) << "() {\n";
  os << function.block();
  if (function.has_return_stmt()) {
    os << "return " << function.return_stmt() << ";\n";
  } else {
    os << "return;\n";
  }
  os << "}";
  return os;
}

std::ostream& operator<<(std::ostream& os, const fuzzing::Shader& shader) {
  for (const fuzzing::Declare& declare : shader.declarations()) {
    GetDeclaration(os, declare, /*is_global=*/true);
  }
  int i = 0;
  for (const fuzzing::Function& function : shader.functions()) {
    os << function;
    if (i < shader.functions().size() - 1) {
      os << "\n";
    }
    i++;
  }
  return os;
}

std::string GetShader(const fuzzing::Shader& shader) {
  if (shader.functions().empty()) {
    return "";
  }
  std::ostringstream os;
  os << "#version 300 es\n";
  os << shader;
  return os.str();
}

}  // namespace gl_lpm_fuzzer
