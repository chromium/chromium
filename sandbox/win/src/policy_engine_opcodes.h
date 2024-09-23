// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef SANDBOX_WIN_SRC_POLICY_ENGINE_OPCODES_H_
#define SANDBOX_WIN_SRC_POLICY_ENGINE_OPCODES_H_

#include <stddef.h>
#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "sandbox/win/src/policy_engine_params.h"

// The low-level policy is implemented using the concept of policy 'opcodes'.
// An opcode is a structure that contains enough information to perform one
// comparison against one single input parameter. For example, an opcode can
// encode just one of the following comparison:
//
// - Is input parameter 3 not equal to nullptr?
// - Does input parameter 2 start with L"c:\\"?
// - Is input parameter 5, bit 3 is equal 1?
//
// Each opcode is in fact equivalent to a function invocation where all
// the parameters are known by the opcode except one. So say you have a
// function of this form:
//      bool fn(a, b, c, d)  with 4 arguments
//
// Then an opcode is:
//      op(fn, b, c, d)
// Which stores the function to call and its 3 last arguments
//
// Then and opcode evaluation is:
//      op.eval(a)  ------------------------> fn(a,b,c,d)
//                        internally calls
//
// The idea is that complex policy rules can be split into streams of
// opcodes which are evaluated in sequence. The evaluation is done in
// groups of opcodes that have N comparison opcodes plus 1 action opcode:
//
// [comparison 1][comparison 2]...[comparison N][action][comparison 1]...
//    ----- evaluation order----------->
//
// Each opcode group encodes one high-level policy rule. The rule applies
// only if all the conditions on the group evaluate to true. The action
// opcode contains the policy outcome for that particular rule.
//
// Note that this header contains the main building blocks of low-level policy
// but not the low level policy class.
namespace sandbox {

// These are the possible policy outcomes. Note that some of them might
// not apply and can be removed. Also note that The following values only
// specify what to do, not how to do it and it is acceptable given specific
// cases to ignore the policy outcome.
enum EvalResult {
  // Comparison opcode values:
  EVAL_TRUE,   // Opcode condition evaluated true.
  EVAL_FALSE,  // Opcode condition evaluated false.
  EVAL_ERROR,  // Opcode condition generated an error while evaluating.
  // Action opcode values:
  ASK_BROKER,   // The target must generate an IPC to the broker. On the broker
                // side, this means grant access to the resource.
  DENY_ACCESS,  // No access granted to the resource.
  SIGNAL_ALARM,    // Unusual activity. Generate an alarm.
  FAKE_SUCCESS,    // Do not call original function. Just return 'success'.
  FAKE_ACCESS_DENIED,  // Do not call original function. Just return 'denied'
                       // and do not do IPC.
};

// The following are the implemented opcodes. uint16_t purely to pack nicely.
enum OpcodeID : uint16_t {
  OP_ALWAYS_FALSE,        // Evaluates to false (EVAL_FALSE).
  OP_ALWAYS_TRUE,         // Evaluates to true (EVAL_TRUE).
  OP_NUMBER_MATCH,        // Match a 32-bit integer as n == a.
  OP_NUMBER_AND_MATCH,    // Match using bitwise AND; as in: n & a != 0.
  OP_WSTRING_MATCH,       // Match a string for equality.
  OP_ACTION               // Evaluates to an action opcode.
};

// Options that apply to every opcode. They are specified when creating
// each opcode using OpcodeFactory::MakeOpXXXXX() family of functions
// Do nothing special.
const uint32_t kPolNone = 0;

// Convert EVAL_TRUE into EVAL_FALSE and vice-versa. This allows to express
// negated conditions such as if ( a && !b).
const uint32_t kPolNegateEval = 1;

// Zero the MatchContext context structure. This happens after the opcode
// is evaluated.
const uint32_t kPolClearContext = 2;

// Use OR when evaluating this set of opcodes. The policy evaluator by default
// uses AND when evaluating. Very helpful when
// used with kPolNegateEval. For example if you have a condition best expressed
// as if(! (a && b && c)), the use of this flags allows it to be expressed as
// if ((!a) || (!b) || (!c)).
const uint32_t kPolUseOREval = 4;

// Keeps the evaluation state between opcode evaluations. This is used
// for string matching where the next opcode needs to continue matching
// from the last character position from the current opcode. The match
// context is preserved across opcode evaluation unless an opcode specifies
// as an option kPolClearContext.
struct MatchContext {
  size_t position;
  uint32_t options;

  MatchContext() { Clear(); }

  void Clear() {
    position = 0;
    options = 0;
  }
};

// Models a policy opcode; that is a condition evaluation were all the
// arguments but one are stored in objects of this class. Use OpcodeFactory
// to create objects of this type.
// This class is just an implementation artifact and not exposed to the
// API clients or visible in the intercepted service. Internally, an
// opcode is just:
//  - An integer that identifies the actual opcode.
//  - An index to indicate which one is the input argument
//  - An array of arguments.
// While an OO hierarchy of objects would have been a natural choice, the fact
// that 1) this code can execute before the CRT is loaded, presents serious
// problems in terms of guarantees about the actual state of the vtables and
// 2) because the opcode objects are generated in the broker process, we need to
// use plain objects. To preserve some minimal type safety templates are used
// when possible.
class PolicyOpcode {
  friend class OpcodeFactory;

 public:
  // Evaluates the opcode. For a typical comparison opcode the return value
  // is EVAL_TRUE or EVAL_FALSE. If there was an error in the evaluation the
  // the return is EVAL_ERROR. If the opcode is an action opcode then the
  // return can take other values such as ASK_BROKER.
  // parameters: An array of all input parameters. This argument is normally
  // created by the macros POLPARAMS_BEGIN() POLPARAMS_END.
  // count: The number of parameters passed as first argument.
  // match: The match context that is persisted across the opcode evaluation
  // sequence.
  EvalResult Evaluate(const ParameterSet* parameters,
                      size_t count,
                      MatchContext* match);

  // Retrieves a stored argument by index. Valid index values are
  // from 0 to < kArgumentCount.
  template <typename T>
  void GetArgument(size_t index, T* argument) const {
    static_assert(sizeof(T) <= sizeof(arguments_[0]), "invalid size");
    *argument = *reinterpret_cast<const T*>(&arguments_[index].mem);
  }

  // Sets a stored argument by index. Valid index values are
  // from 0 to < kArgumentCount.
  template <typename T>
  void SetArgument(size_t index, const T& argument) {
    static_assert(sizeof(T) <= sizeof(arguments_[0]), "invalid size");
    *reinterpret_cast<T*>(&arguments_[index].mem) = argument;
  }

  // Retrieves the actual address of a string argument. When using
  // GetArgument() to retrieve an index that contains a string, the returned
  // value is just an offset to the actual string.
  // index: the stored string index. Valid values are from 0
  // to < kArgumentCount.
  const wchar_t* GetRelativeString(size_t index) const {
    ptrdiff_t str_delta = 0;
    GetArgument(index, &str_delta);
    const char* delta = reinterpret_cast<const char*>(this) + str_delta;
    return reinterpret_cast<const wchar_t*>(delta);
  }

  // Returns true if this opcode is an action opcode without actually
  // evaluating it. Used to do a quick scan forward to the next opcode group.
  bool IsAction() const { return (OP_ACTION == opcode_id_); }

  // Returns the opcode type.
  OpcodeID GetID() const { return opcode_id_; }

  // Returns the stored options such as kPolNegateEval and others.
  uint32_t GetOptions() const { return options_; }

  // Sets the stored options such as kPolNegateEval.
  void SetOptions(uint32_t options) { options_ = options; }

  // Returns the parameter of the function the opcode concerns.
  uint8_t GetParameter() const { return parameter_; }

 private:
  static const size_t kArgumentCount = 4;  // The number of supported argument.

  struct OpcodeArgument {
    UINT_PTR mem;
  };

  // Better define placement new in the class instead of relying on the
  // global definition which seems to be fubared.
  void* operator new(size_t, void* location) { return location; }

  // Helper function to evaluate the opcode. The parameters have the same
  // meaning that in Evaluate().
  EvalResult EvaluateHelper(const ParameterSet* parameters,
                            MatchContext* match);
  OpcodeID opcode_id_;
  // Used a boolean field but provided as a uint8_t to maintain packing.
  uint8_t has_param_;
  // Not used if has_param_ is false.
  uint8_t parameter_;
  uint32_t options_;
  OpcodeArgument arguments_[PolicyOpcode::kArgumentCount];
};

// Opcodes that do string comparisons take a parameter that is the starting
// position to perform the comparison so we can do substring matching. There
// are two special values:
//
// Start from the current position and compare strings advancing forward until
// a match is found if any. Similar to CRT strstr().
const int kSeekForward = -1;
// Perform a match with the end of the string. It only does a single comparison.
const int kSeekToEnd = 0xfffff;

// A PolicyBuffer is a variable size structure that contains all the opcodes
// that are to be created or evaluated in sequence.
struct PolicyBuffer {
  size_t opcode_count;
  PolicyOpcode opcodes[1];
};

// Helper class to create any opcode sequence. This class is normally invoked
// only by the high level policy module or when you need to handcraft a special
// policy.
// The factory works by creating the opcodes using a chunk of memory given
// in the constructor. The opcodes themselves are allocated from the beginning
// (top) of the memory, while any string that an opcode needs is allocated from
// the end (bottom) of the memory.
//
// In essence:
//
//   low address ---> [opcode 1]
//                    [opcode 2]
//                    [opcode 3]
//                    |        | <--- memory_top_
//                    | free   |
//                    |        |
//                    |        | <--- memory_bottom_
//                    [string 1]
//   high address --> [string 2]
//
// Note that this class does not keep track of the number of opcodes made and
// it is designed to be a building block for low-level policy.
//
// Note that any of the MakeOpXXXXX member functions below can return nullptr on
// failure. When that happens opcode sequence creation must be aborted.
class OpcodeFactory {
 public:
  // memory: base pointer to a chunk of memory where the opcodes are created.
  // memory_size: the size in bytes of the memory chunk.
  OpcodeFactory(char* memory, size_t memory_size) : memory_top_(memory) {
    memory_bottom_ = &memory_top_[memory_size];
  }

  // policy: contains the raw memory where the opcodes are created.
  // memory_size: contains the actual size of the policy argument.
  OpcodeFactory(PolicyBuffer* policy, size_t memory_size) {
    memory_top_ = reinterpret_cast<char*>(&policy->opcodes[0]);
    memory_bottom_ = &memory_top_[memory_size];
  }

  OpcodeFactory(const OpcodeFactory&) = delete;
  OpcodeFactory& operator=(const OpcodeFactory&) = delete;

  // Returns the available memory to make opcodes.
  size_t memory_size() const;

  // Creates an OpAlwaysFalse opcode.
  PolicyOpcode* MakeOpAlwaysFalse(uint32_t options);

  // Creates an OpAlwaysFalse opcode.
  PolicyOpcode* MakeOpAlwaysTrue(uint32_t options);

  // Creates an OpAction opcode.
  // action: The action to return when Evaluate() is called.
  PolicyOpcode* MakeOpAction(EvalResult action, uint32_t options);

  // Creates an OpNumberMatch opcode.
  // selected_param: index of the input argument. It must be a uint32_t or the
  // evaluation result will generate a EVAL_ERROR.
  // match: the number to compare against the selected_param.
  PolicyOpcode* MakeOpNumberMatch(uint8_t selected_param,
                                  uint32_t match,
                                  uint32_t options);

  // Creates an OpNumberMatch opcode (void pointers are cast to numbers).
  // selected_param: index of the input argument. It must be an void* or the
  // evaluation result will generate a EVAL_ERROR.
  // match: the pointer numeric value to compare against selected_param.
  PolicyOpcode* MakeOpVoidPtrMatch(uint8_t selected_param,
                                   const void* match,
                                   uint32_t options);

  // Creates an OpWStringMatch opcode using the raw memory passed in the ctor.
  // selected_param: index of the input argument. It must be a wide string
  // pointer or the evaluation result will generate a EVAL_ERROR.
  // match_str: string to compare against selected_param.
  // start_position: when its value is from 0 to < 0x7fff it indicates an
  // offset from the selected_param string where to perform the comparison. If
  // the value is SeekForward  then a substring search is performed. If the
  // value is SeekToEnd the comparison is performed against the last part of
  // the selected_param string.
  // Note that the range in the position (0 to 0x7fff) is dictated by the
  // current implementation.
  // All comparisons are case-insensitive.
  PolicyOpcode* MakeOpWStringMatch(uint8_t selected_param,
                                   const wchar_t* match_str,
                                   int start_position,
                                   uint32_t options,
                                   bool final_token);

  // Creates an OpNumberAndMatch opcode using the raw memory passed in the ctor.
  // selected_param: index of the input argument. It must be uint32_t or the
  // evaluation result will generate a EVAL_ERROR.
  // match: the value to bitwise AND against selected_param.
  PolicyOpcode* MakeOpNumberAndMatch(uint8_t selected_param,
                                     uint32_t match,
                                     uint32_t options);

 private:
  // Constructs the common part of every opcode. selected_param is the index
  // of the input param to use when evaluating the opcode.
  PolicyOpcode* MakeBase(OpcodeID opcode_id, uint32_t options);
  PolicyOpcode* MakeBase(OpcodeID opcode_id,
                         uint32_t options,
                         uint8_t selected_param);

  // Allocates (and copies) a string (of size length) inside the buffer and
  // returns the displacement with respect to start.
  ptrdiff_t AllocRelative(void* start, const wchar_t* str, size_t length);

  // Points to the lowest currently available address of the memory
  // used to make the opcodes. This pointer increments as opcodes are made.
  raw_ptr<char, AllowPtrArithmetic | DanglingUntriaged> memory_top_;

  // Points to the highest currently available address of the memory
  // used to make the opcodes. This pointer decrements as opcode strings are
  // allocated.
  raw_ptr<char, AllowPtrArithmetic | DanglingUntriaged> memory_bottom_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_POLICY_ENGINE_OPCODES_H_
