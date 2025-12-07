// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Counters for assessing CPU utilization.  Each field is monotonically
// increasing while the processor is powered on.  Values are in milliseconds.
dictionary CpuTime {
  // The cumulative time used by userspace programs on this processor.
  required double user;

  // The cumulative time used by kernel programs on this processor.
  required double kernel;

  // The cumulative time spent idle by this processor.
  required double idle;

  // The total cumulative time for this processor.  This value is equal to
  // user + kernel + idle.
  required double total;
};

dictionary ProcessorInfo {
  // Cumulative usage info for this logical processor.
  required CpuTime usage;
};

dictionary CpuInfo {
  // The number of logical processors.
  required long numOfProcessors;

  // The architecture name of the processors.
  required DOMString archName;

  // The model name of the processors.
  required DOMString modelName;

  // A set of feature codes indicating some of the processor's capabilities.
  // The currently supported codes are "mmx", "sse", "sse2", "sse3", "ssse3",
  // "sse4_1", "sse4_2", and "avx".
  required sequence<DOMString> features;

  // Information about each logical processor.
  required sequence<ProcessorInfo> processors;

  // List of CPU temperature readings from each thermal zone of the CPU.
  // Temperatures are in degrees Celsius.
  //
  // <b>Currently supported on Chrome OS only.</b>
  required sequence<double> temperatures;
};

// Use the <code>system.cpu</code> API to query CPU metadata.
interface CPU {
  // Queries basic CPU information of the system.
  // |PromiseValue|: info
  [requiredCallback] static Promise<CpuInfo> getInfo();
};

partial interface System {
  static attribute CPU cpu;
};

partial interface Browser {
  static attribute System system;
};
