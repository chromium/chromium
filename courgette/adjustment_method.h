// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_ADJUSTMENT_METHOD_H_
#define COURGETTE_ADJUSTMENT_METHOD_H_

namespace courgette {

class AssemblyProgram;

class AdjustmentMethod {
 public:
  // Factory methods for making adjusters.

  // Returns the adjustment method used in production.
  static AdjustmentMethod* MakeProductionAdjustmentMethod() {
    return MakeShingleAdjustmentMethod();
  }

  // Returns and adjustement method that makes no adjustments.
  static AdjustmentMethod* MakeNullAdjustmentMethod();

  // Returns the original adjustment method.
  static AdjustmentMethod* MakeTrieAdjustmentMethod();

  // Returns the new shingle tiling adjustment method.
  static AdjustmentMethod* MakeShingleAdjustmentMethod();

  AdjustmentMethod(const AdjustmentMethod&) = delete;
  AdjustmentMethod& operator=(const AdjustmentMethod&) = delete;

  // AdjustmentMethod interface:

  // Adjusts |program| to increase similarity to |model|.  |program| can be
  // changed in any way provided that it still produces the same output when
  // assembled.
  virtual bool Adjust(const AssemblyProgram& model,
                      AssemblyProgram* program) = 0;

  // Deletes 'this' adjustment method.
  virtual void Destroy();

 protected:
  AdjustmentMethod() {}
  virtual ~AdjustmentMethod() {}
};

}  // namespace courgette
#endif  // COURGETTE_ADJUSTMENT_METHOD_H_
