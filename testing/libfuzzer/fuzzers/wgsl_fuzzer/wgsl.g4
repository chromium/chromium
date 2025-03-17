// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// DISCLAIMER: this file does not fully describe the WGSL grammar, and is only
// meant to be used with FuzzTest to exercise tint's wgsl parser.

grammar wgsl;

wgsl: translation_unit ;

translation_unit :
  global_directive * ( global_decl | global_assert | ';' ) * EOF
;

global_directive :
  diagnostic_directive
| enable_directive
| requires_directive
;

global_decl :
  global_variable_decl ';'
| global_value_decl ';'
| type_alias_decl ';'
| struct_decl
| function_decl
;

global_assert :
  const_assert ';'
;

bool_literal :
  'true'
| 'false'
;

int_literal :
  DECIMAL_INT_LITERAL
| HEX_INT_LITERAL
;

DECIMAL_INT_LITERAL :
  /[0][iu]?/
| /[1-9][0-9]*[iu]?/
;

HEX_INT_LITERAL :
  /[0][xX][0-9a-fA-F]+[iu]?/
;

float_literal :
  DECIMAL_FLOAT_LITERAL
| HEX_FLOAT_LITERAL
;

DECIMAL_FLOAT_LITERAL :
  /[0][fh]/
| /[1-9][0-9]*[fh]/
| /[0-9]*[.][0-9]+([eE][+-]?[0-9]+)?[fh]?/
| /[0-9]+[.][0-9]*([eE][+-]?[0-9]+)?[fh]?/
| /[0-9]+[eE][+-]?[0-9]+[fh]?/
;

HEX_FLOAT_LITERAL :
  /[0][xX][0-9a-fA-F]*[.][0-9a-fA-F]+([pP][+-]?[0-9]+[fh]?)?/
| /[0][xX][0-9a-fA-F]+[.][0-9a-fA-F]*([pP][+-]?[0-9]+[fh]?)?/
| /[0][xX][0-9a-fA-F]+[pP][+-]?[0-9]+[fh]?/
;

diagnostic_directive :
  'diagnostic' diagnostic_control ';'
;

literal :
  int_literal
| float_literal
| bool_literal
;

ident :
  IDENT_PATTERN_TOKEN
;

member_ident :
  IDENT_PATTERN_TOKEN
;

diagnostic_name_token :
  IDENT_PATTERN_TOKEN
;

diagnostic_rule_name :
  diagnostic_name_token
| diagnostic_name_token '.' diagnostic_name_token
;

template_args_start : 
  '<'
;

template_args_end:
  '>'
;

template_list :
  template_args_start template_arg_comma_list template_args_end
;

template_arg_comma_list :
  template_arg_expression ( ',' template_arg_expression ) * ',' ?
;

template_arg_expression :
  expression
;

align_attr :
  '@' 'align' '(' expression ',' ? ')'
;

binding_attr :
  '@' 'binding' '(' expression ',' ? ')'
;

blend_src_attr :
  '@' 'blend_src' '(' expression ',' ? ')'
;

builtin_attr :
  '@' 'builtin' '(' builtin_value_name ',' ? ')'
;

builtin_value_name :
  IDENT_PATTERN_TOKEN
;

const_attr :
  '@' 'const'
;

diagnostic_attr :
  '@' 'diagnostic' diagnostic_control
;

group_attr :
  '@' 'group' '(' expression ',' ? ')'
;

id_attr :
  '@' 'id' '(' expression ',' ? ')'
;

interpolate_attr :
  '@' 'interpolate' '(' interpolate_type_name ',' ? ')'
| '@' 'interpolate' '(' interpolate_type_name ',' interpolate_sampling_name ',' ? ')'
;

interpolate_type_name :
  IDENT_PATTERN_TOKEN
;

interpolate_sampling_name :
  IDENT_PATTERN_TOKEN
;

invariant_attr :
  '@' 'invariant'
;

location_attr :
  '@' 'location' '(' expression ',' ? ')'
;

must_use_attr :
  '@' 'must_use'
;

size_attr :
  '@' 'size' '(' expression ',' ? ')'
;

workgroup_size_attr :
  '@' 'workgroup_size' '(' expression ',' ? ')'
| '@' 'workgroup_size' '(' expression ',' expression ',' ? ')'
| '@' 'workgroup_size' '(' expression ',' expression ',' expression ',' ? ')'
;

vertex_attr :
  '@' 'vertex'
;

fragment_attr :
  '@' 'fragment'
;

compute_attr :
  '@' 'compute'
;

attribute :
  '@' IDENT_PATTERN_TOKEN argument_expression_list ?
| align_attr
| binding_attr
| blend_src_attr
| builtin_attr
| const_attr
| diagnostic_attr
| group_attr
| id_attr
| interpolate_attr
| invariant_attr
| location_attr
| must_use_attr
| size_attr
| workgroup_size_attr
| vertex_attr
| fragment_attr
| compute_attr
;

diagnostic_control :
  '(' severity_control_name ',' diagnostic_rule_name ',' ? ')'
;

struct_decl :
  'struct' ident struct_body_decl
;

struct_body_decl :
  '{' struct_member ( ',' struct_member ) * ',' ? '}'
;

struct_member :
  attribute * member_ident ':' type_specifier
;

type_alias_decl :
  'alias' ident '=' type_specifier
;

type_specifier :
  template_elaborated_ident
;

template_elaborated_ident :
  ident template_list ?
;

variable_or_value_statement :
  variable_decl
| variable_decl '=' expression
| 'let' optionally_typed_ident '=' expression
| 'const' optionally_typed_ident '=' expression
;

variable_decl :
  'var' template_list ? optionally_typed_ident
;

optionally_typed_ident :
  ident ( ':' type_specifier ) ?
;

global_variable_decl :
  attribute * variable_decl ( '=' expression ) ?
;

global_value_decl :
  'const' optionally_typed_ident '=' expression
| attribute * 'override' optionally_typed_ident ( '=' expression ) ?
;

primary_expression :
  template_elaborated_ident
| call_expression
| literal
| paren_expression
;

call_expression :
  call_phrase
;

call_phrase :
  template_elaborated_ident argument_expression_list
;

paren_expression :
  '(' expression ')'
;

argument_expression_list :
  '(' expression_comma_list ? ')'
;

expression_comma_list :
  expression ( ',' expression ) * ',' ?
;

component_or_swizzle_specifier :
  '[' expression ']' component_or_swizzle_specifier ?
| '.' member_ident component_or_swizzle_specifier ?
| '.' SWIZZLE_NAME component_or_swizzle_specifier ?
;

unary_expression :
  singular_expression
| '-' unary_expression
| '!' unary_expression
| '~' unary_expression
| '*' unary_expression
| '&' unary_expression
;

singular_expression :
  primary_expression component_or_swizzle_specifier ?
;

lhs_expression :
  core_lhs_expression component_or_swizzle_specifier ?
| '*' lhs_expression
| '&' lhs_expression
;

core_lhs_expression :
  ident
| '(' lhs_expression ')'
;

multiplicative_expression :
  unary_expression
| multiplicative_expression multiplicative_operator unary_expression
;

multiplicative_operator :
  '*'
| '/'
| '%'
;

additive_expression :
  multiplicative_expression
| additive_expression additive_operator multiplicative_expression
;

additive_operator :
  '+'
| '-'
;

shift_left:
  '<<'
;

shift_right:
  '>>'
;

less_than:
  '<'
;

greater_than:
  '>'
;

less_than_equal:
  '<='
;

greater_than_equal:
  '>='
;

shift_expression :
  additive_expression
| unary_expression shift_left unary_expression
| unary_expression shift_right unary_expression
;

relational_expression :
  shift_expression
| shift_expression less_than shift_expression
| shift_expression greater_than shift_expression
| shift_expression less_than_equal shift_expression
| shift_expression greater_than_equal shift_expression
| shift_expression '==' shift_expression
| shift_expression '!=' shift_expression
;

short_circuit_and_expression :
  relational_expression
| short_circuit_and_expression '&&' relational_expression
;

short_circuit_or_expression :
  relational_expression
| short_circuit_or_expression '||' relational_expression
;

binary_or_expression :
  unary_expression
| binary_or_expression '|' unary_expression
;

binary_and_expression :
  unary_expression
| binary_and_expression '&' unary_expression
;

binary_xor_expression :
  unary_expression
| binary_xor_expression '^' unary_expression
;

bitwise_expression :
  binary_and_expression '&' unary_expression
| binary_or_expression '|' unary_expression
| binary_xor_expression '^' unary_expression
;

expression :
  relational_expression
| short_circuit_or_expression '||' relational_expression
| short_circuit_and_expression '&&' relational_expression
| bitwise_expression
;

compound_statement :
  attribute * '{' statement * '}'
;

assignment_statement :
  lhs_expression ( '=' | compound_assignment_operator ) expression
| '_' '=' expression
;

shift_right_assign :
  '>>='
;

shift_left_assign :
  '<<='
;

compound_assignment_operator :
  '+='
| '-='
| '*='
| '/='
| '%='
| '&='
| '|='
| '^='
| shift_right_assign
| shift_left_assign
;

increment_statement :
  lhs_expression '++'
;

decrement_statement :
  lhs_expression '--'
;

if_statement :
  attribute * if_clause else_if_clause * else_clause ?
;

if_clause :
  'if' expression compound_statement
;

else_if_clause :
  'else' 'if' expression compound_statement
;

else_clause :
  'else' compound_statement
;

switch_statement :
  attribute * 'switch' expression switch_body
;

switch_body :
  attribute * '{' switch_clause + '}'
;

switch_clause :
  case_clause
| default_alone_clause
;

case_clause :
  'case' case_selectors ':' ? compound_statement
;

default_alone_clause :
  'default' ':' ? compound_statement
;

case_selectors :
  case_selector ( ',' case_selector ) * ',' ?
;

case_selector :
  'default'
| expression
;

loop_statement :
  attribute * 'loop' attribute * '{' statement * continuing_statement ? '}'
;

for_statement :
  attribute * 'for' '(' for_header ')' compound_statement
;

for_header :
  for_init ? ';' expression ? ';' for_update ?
;

for_init :
  variable_or_value_statement
| variable_updating_statement
| func_call_statement
;

for_update :
  variable_updating_statement
| func_call_statement
;

while_statement :
  attribute * 'while' expression compound_statement
;

break_statement :
  'break'
;

break_if_statement :
  'break' 'if' expression ';'
;

continue_statement :
  'continue'
;

continuing_statement :
  'continuing' continuing_compound_statement
;

continuing_compound_statement :
  attribute * '{' statement * break_if_statement ? '}'
;

return_statement :
  'return' expression ?
;

func_call_statement :
  call_phrase
;

const_assert :
  'const_assert' expression
;

assert_statement :
  const_assert
;

statement :
  ';'
| return_statement ';'
| if_statement
| switch_statement
| loop_statement
| for_statement
| while_statement
| func_call_statement ';'
| variable_or_value_statement ';'
| break_statement ';'
| continue_statement ';'
| 'discard' ';'
| variable_updating_statement ';'
| compound_statement
| assert_statement ';'
;

variable_updating_statement :
  assignment_statement
| increment_statement
| decrement_statement
;

function_decl :
  attribute * function_header compound_statement
;

function_header :
  'fn' ident '(' param_list ? ')' ( '->' attribute * template_elaborated_ident ) ?
;

param_list :
  param ( ',' param ) * ',' ?
;

param :
  attribute * ident ':' type_specifier
;

enable_directive :
  'enable' enable_extension_list ';'
;

enable_extension_list :
  enable_extension_name ( ',' enable_extension_name ) * ',' ?
;

requires_directive :
  'requires' software_extension_list ';'
;

software_extension_list :
  software_extension_name ( ',' software_extension_name ) * ',' ?
;

enable_extension_name :
  IDENT_PATTERN_TOKEN
;

software_extension_name :
  IDENT_PATTERN_TOKEN
;

IDENT_PATTERN_TOKEN :
  /([_a-zA-Z][_0-9A-Za-z]+)|([a-zA-Z])/
;

severity_control_name :
  IDENT_PATTERN_TOKEN
;

SWIZZLE_NAME :
  /[rgba]/
| /[rgba][rgba]/
| /[rgba][rgba][rgba]/
| /[rgba][rgba][rgba][rgba]/
| /[xyzw]/
| /[xyzw][xyzw]/
| /[xyzw][xyzw][xyzw]/
| /[xyzw][xyzw][xyzw][xyzw]/
;

Whitespace :
  [ \t]+ -> channel(HIDDEN)
;
