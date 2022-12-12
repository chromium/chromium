/*
 * Copyright 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* === NOTA BENE ===
 * If you modify this file, you must run bison to regenerate the corresponding
 * .cc and .h files. From chromium's root directory, run the following command
 * on a system with a modern version of bison (>= 3.4.1):
 *
 *   $ third_party/blink/renderer/build/scripts/rule_bison.py \
 *       third_party/blink/renderer/core/xml/xpath_grammar.y \
 *       third_party/blink/renderer/core/xml/ \
 *       bison
 *
 * This process is not automated because newer bison releases have diverged from
 * (1) the version included with Xcode and (2) the Windows binary checked into
 * //third_party/bison. See https://crbug.com/1028421.
 */

%require "3.4"
%language "c++"

%code requires {

#include "third_party/blink/renderer/platform/heap/persistent.h"

}

%code {
#if defined(__clang__)
// Clang warns that the variable 'yynerrs_' is set but not used.
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#endif
}


%{

#include "third_party/blink/renderer/core/xml/xpath_functions.h"
#include "third_party/blink/renderer/core/xml/xpath_parser.h"
#include "third_party/blink/renderer/core/xml/xpath_path.h"
#include "third_party/blink/renderer/core/xml/xpath_predicate.h"
#include "third_party/blink/renderer/core/xml/xpath_step.h"
#include "third_party/blink/renderer/core/xml/xpath_variable_reference.h"

#define YYENABLE_NLS 0
#define YY_EXCEPTIONS 0
#define YYDEBUG 0

using blink::xpath::Step;
%}

%define api.namespace {xpathyy}
%define api.parser.class {YyParser}
%parse-param { blink::xpath::Parser* parser_ }

%define api.value.type variant

%left <blink::xpath::NumericOp::Opcode> kMulOp
%left <blink::xpath::EqTestOp::Opcode> kEqOp kRelOp
%left kPlus kMinus
%left kOr kAnd
%token <blink::xpath::Step::Axis> kAxisName
%token <String> kNodeType kPI kFunctionName kLiteral
%token <String> kVariableReference kNumber
%token kDotDot kSlashSlash
%token <String> kNameTest
%token kXPathError

%type <blink::Persistent<blink::xpath::LocationPath>> LocationPath
%type <blink::Persistent<blink::xpath::LocationPath>> AbsoluteLocationPath
%type <blink::Persistent<blink::xpath::LocationPath>> RelativeLocationPath
%type <blink::Persistent<blink::xpath::Step>> Step
%type <blink::xpath::Step::Axis> AxisSpecifier
%type <blink::Persistent<blink::xpath::Step>> DescendantOrSelf
%type <blink::Persistent<blink::xpath::Step::NodeTest>> NodeTest
%type <blink::Persistent<blink::xpath::Expression>> Predicate
%type <blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>>> OptionalPredicateList
%type <blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Predicate>>>> PredicateList
%type <blink::Persistent<blink::xpath::Step>> AbbreviatedStep
%type <blink::Persistent<blink::xpath::Expression>> Expr
%type <blink::Persistent<blink::xpath::Expression>> PrimaryExpr
%type <blink::Persistent<blink::xpath::Expression>> FunctionCall
%type <blink::Persistent<blink::HeapVector<blink::Member<blink::xpath::Expression>>>> ArgumentList
%type <blink::Persistent<blink::xpath::Expression>> Argument
%type <blink::Persistent<blink::xpath::Expression>> UnionExpr
%type <blink::Persistent<blink::xpath::Expression>> PathExpr
%type <blink::Persistent<blink::xpath::Expression>> FilterExpr
%type <blink::Persistent<blink::xpath::Expression>> OrExpr
%type <blink::Persistent<blink::xpath::Expression>> AndExpr
%type <blink::Persistent<blink::xpath::Expression>> EqualityExpr
%type <blink::Persistent<blink::xpath::Expression>> RelationalExpr
%type <blink::Persistent<blink::xpath::Expression>> AdditiveExpr
%type <blink::Persistent<blink::xpath::Expression>> MultiplicativeExpr
%type <blink::Persistent<blink::xpath::Expression>> UnaryExpr

%code {

static int yylex(xpathyy::YyParser::semantic_type* yylval) {
  return blink::xpath::Parser::Current()->Lex(yylval);
}

namespace xpathyy {
void YyParser::error(const std::string&) { }
}

}

%%

Expr:
    OrExpr
    {
      parser_->top_expr_ = $1;
      $$ = $1;
    }
    ;

LocationPath:
    RelativeLocationPath
    {
      $$ = $1;
      $$->SetAbsolute(false);
    }
    |
    AbsoluteLocationPath
    {
      $$ = $1;
      $$->SetAbsolute(true);
    }
    ;

AbsoluteLocationPath:
    '/'
    {
      $$ = blink::MakeGarbageCollected<blink::xpath::LocationPath>();
    }
    |
    '/' RelativeLocationPath
    {
      $$ = $2;
    }
    |
    DescendantOrSelf RelativeLocationPath
    {
      $$ = $2;
      $$->InsertFirstStep($1);
    }
    ;

RelativeLocationPath:
    Step
    {
      $$ = blink::MakeGarbageCollected<blink::xpath::LocationPath>();
      $$->AppendStep($1);
    }
    |
    RelativeLocationPath '/' Step
    {
      $$ = $1;
      $$->AppendStep($3);
    }
    |
    RelativeLocationPath DescendantOrSelf Step
    {
      $$ = $1;
      $$->AppendStep($2);
      $$->AppendStep($3);
    }
    ;

Step:
    NodeTest OptionalPredicateList
    {
      if ($2)
        $$ = blink::MakeGarbageCollected<Step>(Step::kChildAxis, *$1, *$2);
      else
        $$ = blink::MakeGarbageCollected<Step>(Step::kChildAxis, *$1);
    }
    |
    kNameTest OptionalPredicateList
    {
      AtomicString local_name;
      AtomicString namespace_uri;
      if (!parser_->ExpandQName($1, local_name, namespace_uri)) {
        parser_->got_namespace_error_ = true;
        YYABORT;
      }

      if ($2)
        $$ = blink::MakeGarbageCollected<Step>(Step::kChildAxis, Step::NodeTest(Step::NodeTest::kNameTest, local_name, namespace_uri), *$2);
      else
        $$ = blink::MakeGarbageCollected<Step>(Step::kChildAxis, Step::NodeTest(Step::NodeTest::kNameTest, local_name, namespace_uri));
    }
    |
    AxisSpecifier NodeTest OptionalPredicateList
    {
      if ($3)
        $$ = blink::MakeGarbageCollected<Step>($1, *$2, *$3);
      else
        $$ = blink::MakeGarbageCollected<Step>($1, *$2);
    }
    |
    AxisSpecifier kNameTest OptionalPredicateList
    {
      AtomicString local_name;
      AtomicString namespace_uri;
      if (!parser_->ExpandQName($2, local_name, namespace_uri)) {
        parser_->got_namespace_error_ = true;
        YYABORT;
      }

      if ($3)
        $$ = blink::MakeGarbageCollected<Step>($1, Step::NodeTest(Step::NodeTest::kNameTest, local_name, namespace_uri), *$3);
      else
        $$ = blink::MakeGarbageCollected<Step>($1, Step::NodeTest(Step::NodeTest::kNameTest, local_name, namespace_uri));
    }
    |
    AbbreviatedStep
    ;

AxisSpecifier:
    kAxisName
    |
    '@'
    {
      $$ = Step::kAttributeAxis;
    }
    ;

NodeTest:
    kNodeType '(' ')'
    {
      if ($1 == "node")
        $$ = blink::MakeGarbageCollected<Step::NodeTest>(Step::NodeTest::kAnyNodeTest);
      else if ($1 == "text")
        $$ = blink::MakeGarbageCollected<Step::NodeTest>(Step::NodeTest::kTextNodeTest);
      else if ($1 == "comment")
        $$ = blink::MakeGarbageCollected<Step::NodeTest>(Step::NodeTest::kCommentNodeTest);
    }
    |
    kPI '(' ')'
    {
      $$ = blink::MakeGarbageCollected<Step::NodeTest>(Step::NodeTest::kProcessingInstructionNodeTest);
    }
    |
    kPI '(' kLiteral ')'
    {
      $$ = blink::MakeGarbageCollected<Step::NodeTest>(Step::NodeTest::kProcessingInstructionNodeTest, $3.StripWhiteSpace());
    }
    ;

OptionalPredicateList:
    /* empty */
    {
      $$ = nullptr;
    }
    |
    PredicateList
    {
      $$ = $1;
    }
    ;

PredicateList:
    Predicate
    {
      $$ = blink::MakeGarbageCollected<blink::HeapVector<blink::Member<blink::xpath::Predicate>>>();
      $$->push_back(blink::MakeGarbageCollected<blink::xpath::Predicate>($1));
    }
    |
    PredicateList Predicate
    {
      $$ = $1;
      $$->push_back(blink::MakeGarbageCollected<blink::xpath::Predicate>($2));
    }
    ;

Predicate:
    '[' Expr ']'
    {
      $$ = $2;
    }
    ;

DescendantOrSelf:
    kSlashSlash
    {
      $$ = blink::MakeGarbageCollected<Step>(Step::kDescendantOrSelfAxis, Step::NodeTest(Step::NodeTest::kAnyNodeTest));
    }
    ;

AbbreviatedStep:
    '.'
    {
      $$ = blink::MakeGarbageCollected<Step>(Step::kSelfAxis, Step::NodeTest(Step::NodeTest::kAnyNodeTest));
    }
    |
    kDotDot
    {
      $$ = blink::MakeGarbageCollected<Step>(Step::kParentAxis, Step::NodeTest(Step::NodeTest::kAnyNodeTest));
    }
    ;

PrimaryExpr:
    kVariableReference
    {
      $$ = blink::MakeGarbageCollected<blink::xpath::VariableReference>($1);
    }
    |
    '(' Expr ')'
    {
      $$ = $2;
    }
    |
    kLiteral
    {
      $$ = blink::MakeGarbageCollected<blink::xpath::StringExpression>($1);
    }
    |
    kNumber
    {
      $$ = blink::MakeGarbageCollected<blink::xpath::Number>($1.ToDouble());
    }
    |
    FunctionCall
    ;

FunctionCall:
    kFunctionName '(' ')'
    {
      $$ = blink::xpath::CreateFunction($1);
      if (!$$)
        YYABORT;
    }
    |
    kFunctionName '(' ArgumentList ')'
    {
      $$ = blink::xpath::CreateFunction($1, *$3);
      if (!$$)
        YYABORT;
    }
    ;

ArgumentList:
    Argument
    {
      $$ = blink::MakeGarbageCollected<blink::HeapVector<blink::Member<blink::xpath::Expression>>>();
      $$->push_back($1);
    }
    |
    ArgumentList ',' Argument
    {
      $$ = $1;
      $$->push_back($3);
    }
    ;

Argument:
    Expr
    ;

UnionExpr:
    PathExpr
    |
    UnionExpr '|' PathExpr
    {
      $$ = blink::MakeGarbageCollected<blink::xpath::Union>();
      $$->AddSubExpression($1);
      $$->AddSubExpression($3);
    }
    ;

PathExpr:
    LocationPath
    {
      $$ = $1;
    }
    |
    FilterExpr
    |
    FilterExpr '/' RelativeLocationPath
    {
      $3->SetAbsolute(true);
      $$ = blink::MakeGarbageCollected<blink::xpath::Path>($1, $3);
    }
    |
    FilterExpr DescendantOrSelf RelativeLocationPath
    {
      $3->InsertFirstStep($2);
      $3->SetAbsolute(true);
      $$ = blink::MakeGarbageCollected<blink::xpath::Path>($1, $3);
    }
    ;

FilterExpr:
    PrimaryExpr
    |
    PrimaryExpr PredicateList
    {
      $$ = blink::MakeGarbageCollected<blink::xpath::Filter>($1, *$2);
    }
    ;

OrExpr:
    AndExpr
    |
    OrExpr kOr AndExpr
    {
      $$ = blink::MakeGarbageCollected<blink::xpath::LogicalOp>(blink::xpath::LogicalOp::kOP_Or, $1, $3);
    }
    ;

AndExpr:
    EqualityExpr
    |
    AndExpr kAnd EqualityExpr
    {
      $$ = blink::MakeGarbageCollected<blink::xpath::LogicalOp>(blink::xpath::LogicalOp::kOP_And, $1, $3);
    }
    ;

EqualityExpr:
    RelationalExpr
    |
    EqualityExpr kEqOp RelationalExpr
    {
      $$ = blink::MakeGarbageCollected<blink::xpath::EqTestOp>($2, $1, $3);
    }
    ;

RelationalExpr:
    AdditiveExpr
    |
    RelationalExpr kRelOp AdditiveExpr
    {
      $$ = blink::MakeGarbageCollected<blink::xpath::EqTestOp>($2, $1, $3);
    }
    ;

AdditiveExpr:
    MultiplicativeExpr
    |
    AdditiveExpr kPlus MultiplicativeExpr
    {
      $$ = blink::MakeGarbageCollected<blink::xpath::NumericOp>(blink::xpath::NumericOp::kOP_Add, $1, $3);
    }
    |
    AdditiveExpr kMinus MultiplicativeExpr
    {
      $$ = blink::MakeGarbageCollected<blink::xpath::NumericOp>(blink::xpath::NumericOp::kOP_Sub, $1, $3);
    }
    ;

MultiplicativeExpr:
    UnaryExpr
    |
    MultiplicativeExpr kMulOp UnaryExpr
    {
      $$ = blink::MakeGarbageCollected<blink::xpath::NumericOp>($2, $1, $3);
    }
    ;

UnaryExpr:
    UnionExpr
    |
    kMinus UnaryExpr
    {
      $$ = blink::MakeGarbageCollected<blink::xpath::Negative>();
      $$->AddSubExpression($2);
    }
    ;

%%
