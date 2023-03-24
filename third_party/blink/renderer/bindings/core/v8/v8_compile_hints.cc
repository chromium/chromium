// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints.h"

#if BUILDFLAG(ENABLE_V8_COMPILE_HINTS)

#include "base/hash/hash.h"
#include "base/rand_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/bloom_filter.h"

#include <limits>

namespace blink {

namespace {
constexpr int kBloomFilterInt32Count = 1024;
}

std::atomic<bool> V8CompileHints::data_generated_for_this_process_ = false;

V8CompileHints::V8CompileHints(Page* page) : page_(page) {}

void V8CompileHints::RecordScript(Frame* frame,
                                  ExecutionContext* execution_context,
                                  const v8::Local<v8::Script> script,
                                  ScriptState* script_state) {
  if (state_ == State::kDataGenerationFinished || state_ == State::kDisabled) {
    // We've already generated data for this V8CompileHints, or data generation
    // is disabled. Don't record any script compilations happening after it.
    return;
  }
  if (data_generated_for_this_process_) {
    // We've already generated data for some other V8CompileHints, so stop
    // collecting data. The task for data generation might still run.
    state_ = State::kDataGenerationFinished;
    ClearData();
  }

  v8::Isolate* isolate = execution_context->GetIsolate();
  v8::Local<v8::Context> context = script_state->GetContext();

  v8::Local<v8::Value> name_value = script->GetResourceName();
  v8::Local<v8::String> name_string;
  if (!name_value->ToString(context).ToLocal(&name_string)) {
    return;
  }
  auto name_length = name_string->Utf8Length(isolate);
  if (name_length == 0) {
    return;
  }

  // Speed up computing the hashes by hashing the script name only once, and
  // using the hash as "script identifier", then hash "script identifier +
  // function position" pairs. This way retrieving data from the Bloom filter is
  // also fast; we first compute the script name hash, and retrieve data for its
  // functions as we encounter them.

  // We need the hash function to be stable across computers, thus using
  // PersistentHash.
  std::string name_std_string(name_length + 1, '\0');
  name_string->WriteUtf8(isolate, &name_std_string[0]);

  uint32_t script_name_hash =
      base::PersistentHash(name_std_string.c_str(), name_length);

  scripts_.emplace_back(v8::Global<v8::Script>(isolate, script));
  script_name_hashes_.emplace_back(script_name_hash);
}

void V8CompileHints::GenerateData() {
  // Call FeatureList::IsEnabled only once.
  static bool compile_hints_enabled =
      base::FeatureList::IsEnabled(features::kProduceCompileHints);
  if (!compile_hints_enabled) {
    return;
  }

  // Guard against this function getting called repeatedly.
  if (state_ == State::kDataGenerationFinished || state_ == State::kDisabled) {
    return;
  }

  // Stop logging script executions for this page.
  state_ = State::kDataGenerationFinished;

  if (!data_generated_for_this_process_) {
    data_generated_for_this_process_ = SendDataToUkm();
  }

  ClearData();
}

void V8CompileHints::Trace(Visitor* visitor) const {
  visitor->Trace(page_);
}

void V8CompileHints::ClearData() {
  scripts_.clear();
  script_name_hashes_.clear();
}

bool V8CompileHints::SendDataToUkm() {
  Frame* main_frame = page_->MainFrame();
  // Because of OOPIF, the main frame is not necessarily a LocalFrame. We cannot
  // generate good compile hints for those pages, so skip sending them.
  if (!main_frame->IsLocalFrame()) {
    return false;
  }
  ScriptState* script_state =
      ToScriptStateForMainWorld(DynamicTo<LocalFrame>(main_frame));
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  v8::Isolate* isolate = execution_context->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  int total_funcs = 0;

  DCHECK_EQ(scripts_.size(), script_name_hashes_.size());

  // Create a Bloom filter w/ 15 key bits. This results in a Bloom filter
  // containing 2 ^ 15 bits, which equals to 512 64-bit ints.
  constexpr int kBloomFilterKeySize = 15;
  static_assert((1 << kBloomFilterKeySize) / (sizeof(int32_t) * 8) ==
                kBloomFilterInt32Count);
  WTF::BloomFilter<kBloomFilterKeySize> bloom;

  for (wtf_size_t script_ix = 0; script_ix < scripts_.size(); ++script_ix) {
    uint32_t function_position_data[2];
    function_position_data[0] = script_name_hashes_[script_ix];

    v8::Local<v8::Script> script = scripts_[script_ix].Get(isolate);

    std::vector<int> compile_hints = script->GetProducedCompileHints();
    for (int function_position : compile_hints) {
      function_position_data[1] = function_position;
      // We need the hash function to be stable across computers, thus using
      // PersistentHash.
      uint32_t hash =
          base::PersistentHash(function_position_data, 2 * sizeof(int32_t));
      bloom.Add(hash);
      ++total_funcs;
    }
  }

  // Don't clutter the data with elements with less than a threshold amount of
  // functions.
  constexpr int kFunctionCountThreshold = 100;
  if (total_funcs < kFunctionCountThreshold) {
    return false;
  }

  static_assert(sizeof(unsigned) == sizeof(int32_t));
  unsigned* raw_data = (bloom.GetRawData());

  // Add noise to the data.
  for (int i = 0; i < kBloomFilterInt32Count; ++i) {
    AddNoise(&raw_data[i]);
  }

  // Send the data to UKM.
  DCHECK_NE(execution_context->UkmSourceID(), ukm::kInvalidSourceId);
  ukm::UkmRecorder* ukm_recorder = execution_context->UkmRecorder();
  ukm::builders::V8CompileHints_Version2(execution_context->UkmSourceID())
      .SetData0000(static_cast<int64_t>(raw_data[1]) << 32 | raw_data[0])
      .SetData0001(static_cast<int64_t>(raw_data[3]) << 32 | raw_data[2])
      .SetData0002(static_cast<int64_t>(raw_data[5]) << 32 | raw_data[4])
      .SetData0003(static_cast<int64_t>(raw_data[7]) << 32 | raw_data[6])
      .SetData0004(static_cast<int64_t>(raw_data[9]) << 32 | raw_data[8])
      .SetData0005(static_cast<int64_t>(raw_data[11]) << 32 | raw_data[10])
      .SetData0006(static_cast<int64_t>(raw_data[13]) << 32 | raw_data[12])
      .SetData0007(static_cast<int64_t>(raw_data[15]) << 32 | raw_data[14])
      .SetData0008(static_cast<int64_t>(raw_data[17]) << 32 | raw_data[16])
      .SetData0009(static_cast<int64_t>(raw_data[19]) << 32 | raw_data[18])
      .SetData0010(static_cast<int64_t>(raw_data[21]) << 32 | raw_data[20])
      .SetData0011(static_cast<int64_t>(raw_data[23]) << 32 | raw_data[22])
      .SetData0012(static_cast<int64_t>(raw_data[25]) << 32 | raw_data[24])
      .SetData0013(static_cast<int64_t>(raw_data[27]) << 32 | raw_data[26])
      .SetData0014(static_cast<int64_t>(raw_data[29]) << 32 | raw_data[28])
      .SetData0015(static_cast<int64_t>(raw_data[31]) << 32 | raw_data[30])
      .SetData0016(static_cast<int64_t>(raw_data[33]) << 32 | raw_data[32])
      .SetData0017(static_cast<int64_t>(raw_data[35]) << 32 | raw_data[34])
      .SetData0018(static_cast<int64_t>(raw_data[37]) << 32 | raw_data[36])
      .SetData0019(static_cast<int64_t>(raw_data[39]) << 32 | raw_data[38])
      .SetData0020(static_cast<int64_t>(raw_data[41]) << 32 | raw_data[40])
      .SetData0021(static_cast<int64_t>(raw_data[43]) << 32 | raw_data[42])
      .SetData0022(static_cast<int64_t>(raw_data[45]) << 32 | raw_data[44])
      .SetData0023(static_cast<int64_t>(raw_data[47]) << 32 | raw_data[46])
      .SetData0024(static_cast<int64_t>(raw_data[49]) << 32 | raw_data[48])
      .SetData0025(static_cast<int64_t>(raw_data[51]) << 32 | raw_data[50])
      .SetData0026(static_cast<int64_t>(raw_data[53]) << 32 | raw_data[52])
      .SetData0027(static_cast<int64_t>(raw_data[55]) << 32 | raw_data[54])
      .SetData0028(static_cast<int64_t>(raw_data[57]) << 32 | raw_data[56])
      .SetData0029(static_cast<int64_t>(raw_data[59]) << 32 | raw_data[58])
      .SetData0030(static_cast<int64_t>(raw_data[61]) << 32 | raw_data[60])
      .SetData0031(static_cast<int64_t>(raw_data[63]) << 32 | raw_data[62])
      .SetData0032(static_cast<int64_t>(raw_data[65]) << 32 | raw_data[64])
      .SetData0033(static_cast<int64_t>(raw_data[67]) << 32 | raw_data[66])
      .SetData0034(static_cast<int64_t>(raw_data[69]) << 32 | raw_data[68])
      .SetData0035(static_cast<int64_t>(raw_data[71]) << 32 | raw_data[70])
      .SetData0036(static_cast<int64_t>(raw_data[73]) << 32 | raw_data[72])
      .SetData0037(static_cast<int64_t>(raw_data[75]) << 32 | raw_data[74])
      .SetData0038(static_cast<int64_t>(raw_data[77]) << 32 | raw_data[76])
      .SetData0039(static_cast<int64_t>(raw_data[79]) << 32 | raw_data[78])
      .SetData0040(static_cast<int64_t>(raw_data[81]) << 32 | raw_data[80])
      .SetData0041(static_cast<int64_t>(raw_data[83]) << 32 | raw_data[82])
      .SetData0042(static_cast<int64_t>(raw_data[85]) << 32 | raw_data[84])
      .SetData0043(static_cast<int64_t>(raw_data[87]) << 32 | raw_data[86])
      .SetData0044(static_cast<int64_t>(raw_data[89]) << 32 | raw_data[88])
      .SetData0045(static_cast<int64_t>(raw_data[91]) << 32 | raw_data[90])
      .SetData0046(static_cast<int64_t>(raw_data[93]) << 32 | raw_data[92])
      .SetData0047(static_cast<int64_t>(raw_data[95]) << 32 | raw_data[94])
      .SetData0048(static_cast<int64_t>(raw_data[97]) << 32 | raw_data[96])
      .SetData0049(static_cast<int64_t>(raw_data[99]) << 32 | raw_data[98])
      .SetData0050(static_cast<int64_t>(raw_data[101]) << 32 | raw_data[100])
      .SetData0051(static_cast<int64_t>(raw_data[103]) << 32 | raw_data[102])
      .SetData0052(static_cast<int64_t>(raw_data[105]) << 32 | raw_data[104])
      .SetData0053(static_cast<int64_t>(raw_data[107]) << 32 | raw_data[106])
      .SetData0054(static_cast<int64_t>(raw_data[109]) << 32 | raw_data[108])
      .SetData0055(static_cast<int64_t>(raw_data[111]) << 32 | raw_data[110])
      .SetData0056(static_cast<int64_t>(raw_data[113]) << 32 | raw_data[112])
      .SetData0057(static_cast<int64_t>(raw_data[115]) << 32 | raw_data[114])
      .SetData0058(static_cast<int64_t>(raw_data[117]) << 32 | raw_data[116])
      .SetData0059(static_cast<int64_t>(raw_data[119]) << 32 | raw_data[118])
      .SetData0060(static_cast<int64_t>(raw_data[121]) << 32 | raw_data[120])
      .SetData0061(static_cast<int64_t>(raw_data[123]) << 32 | raw_data[122])
      .SetData0062(static_cast<int64_t>(raw_data[125]) << 32 | raw_data[124])
      .SetData0063(static_cast<int64_t>(raw_data[127]) << 32 | raw_data[126])
      .SetData0064(static_cast<int64_t>(raw_data[129]) << 32 | raw_data[128])
      .SetData0065(static_cast<int64_t>(raw_data[131]) << 32 | raw_data[130])
      .SetData0066(static_cast<int64_t>(raw_data[133]) << 32 | raw_data[132])
      .SetData0067(static_cast<int64_t>(raw_data[135]) << 32 | raw_data[134])
      .SetData0068(static_cast<int64_t>(raw_data[137]) << 32 | raw_data[136])
      .SetData0069(static_cast<int64_t>(raw_data[139]) << 32 | raw_data[138])
      .SetData0070(static_cast<int64_t>(raw_data[141]) << 32 | raw_data[140])
      .SetData0071(static_cast<int64_t>(raw_data[143]) << 32 | raw_data[142])
      .SetData0072(static_cast<int64_t>(raw_data[145]) << 32 | raw_data[144])
      .SetData0073(static_cast<int64_t>(raw_data[147]) << 32 | raw_data[146])
      .SetData0074(static_cast<int64_t>(raw_data[149]) << 32 | raw_data[148])
      .SetData0075(static_cast<int64_t>(raw_data[151]) << 32 | raw_data[150])
      .SetData0076(static_cast<int64_t>(raw_data[153]) << 32 | raw_data[152])
      .SetData0077(static_cast<int64_t>(raw_data[155]) << 32 | raw_data[154])
      .SetData0078(static_cast<int64_t>(raw_data[157]) << 32 | raw_data[156])
      .SetData0079(static_cast<int64_t>(raw_data[159]) << 32 | raw_data[158])
      .SetData0080(static_cast<int64_t>(raw_data[161]) << 32 | raw_data[160])
      .SetData0081(static_cast<int64_t>(raw_data[163]) << 32 | raw_data[162])
      .SetData0082(static_cast<int64_t>(raw_data[165]) << 32 | raw_data[164])
      .SetData0083(static_cast<int64_t>(raw_data[167]) << 32 | raw_data[166])
      .SetData0084(static_cast<int64_t>(raw_data[169]) << 32 | raw_data[168])
      .SetData0085(static_cast<int64_t>(raw_data[171]) << 32 | raw_data[170])
      .SetData0086(static_cast<int64_t>(raw_data[173]) << 32 | raw_data[172])
      .SetData0087(static_cast<int64_t>(raw_data[175]) << 32 | raw_data[174])
      .SetData0088(static_cast<int64_t>(raw_data[177]) << 32 | raw_data[176])
      .SetData0089(static_cast<int64_t>(raw_data[179]) << 32 | raw_data[178])
      .SetData0090(static_cast<int64_t>(raw_data[181]) << 32 | raw_data[180])
      .SetData0091(static_cast<int64_t>(raw_data[183]) << 32 | raw_data[182])
      .SetData0092(static_cast<int64_t>(raw_data[185]) << 32 | raw_data[184])
      .SetData0093(static_cast<int64_t>(raw_data[187]) << 32 | raw_data[186])
      .SetData0094(static_cast<int64_t>(raw_data[189]) << 32 | raw_data[188])
      .SetData0095(static_cast<int64_t>(raw_data[191]) << 32 | raw_data[190])
      .SetData0096(static_cast<int64_t>(raw_data[193]) << 32 | raw_data[192])
      .SetData0097(static_cast<int64_t>(raw_data[195]) << 32 | raw_data[194])
      .SetData0098(static_cast<int64_t>(raw_data[197]) << 32 | raw_data[196])
      .SetData0099(static_cast<int64_t>(raw_data[199]) << 32 | raw_data[198])
      .SetData0100(static_cast<int64_t>(raw_data[201]) << 32 | raw_data[200])
      .SetData0101(static_cast<int64_t>(raw_data[203]) << 32 | raw_data[202])
      .SetData0102(static_cast<int64_t>(raw_data[205]) << 32 | raw_data[204])
      .SetData0103(static_cast<int64_t>(raw_data[207]) << 32 | raw_data[206])
      .SetData0104(static_cast<int64_t>(raw_data[209]) << 32 | raw_data[208])
      .SetData0105(static_cast<int64_t>(raw_data[211]) << 32 | raw_data[210])
      .SetData0106(static_cast<int64_t>(raw_data[213]) << 32 | raw_data[212])
      .SetData0107(static_cast<int64_t>(raw_data[215]) << 32 | raw_data[214])
      .SetData0108(static_cast<int64_t>(raw_data[217]) << 32 | raw_data[216])
      .SetData0109(static_cast<int64_t>(raw_data[219]) << 32 | raw_data[218])
      .SetData0110(static_cast<int64_t>(raw_data[221]) << 32 | raw_data[220])
      .SetData0111(static_cast<int64_t>(raw_data[223]) << 32 | raw_data[222])
      .SetData0112(static_cast<int64_t>(raw_data[225]) << 32 | raw_data[224])
      .SetData0113(static_cast<int64_t>(raw_data[227]) << 32 | raw_data[226])
      .SetData0114(static_cast<int64_t>(raw_data[229]) << 32 | raw_data[228])
      .SetData0115(static_cast<int64_t>(raw_data[231]) << 32 | raw_data[230])
      .SetData0116(static_cast<int64_t>(raw_data[233]) << 32 | raw_data[232])
      .SetData0117(static_cast<int64_t>(raw_data[235]) << 32 | raw_data[234])
      .SetData0118(static_cast<int64_t>(raw_data[237]) << 32 | raw_data[236])
      .SetData0119(static_cast<int64_t>(raw_data[239]) << 32 | raw_data[238])
      .SetData0120(static_cast<int64_t>(raw_data[241]) << 32 | raw_data[240])
      .SetData0121(static_cast<int64_t>(raw_data[243]) << 32 | raw_data[242])
      .SetData0122(static_cast<int64_t>(raw_data[245]) << 32 | raw_data[244])
      .SetData0123(static_cast<int64_t>(raw_data[247]) << 32 | raw_data[246])
      .SetData0124(static_cast<int64_t>(raw_data[249]) << 32 | raw_data[248])
      .SetData0125(static_cast<int64_t>(raw_data[251]) << 32 | raw_data[250])
      .SetData0126(static_cast<int64_t>(raw_data[253]) << 32 | raw_data[252])
      .SetData0127(static_cast<int64_t>(raw_data[255]) << 32 | raw_data[254])
      .SetData0128(static_cast<int64_t>(raw_data[257]) << 32 | raw_data[256])
      .SetData0129(static_cast<int64_t>(raw_data[259]) << 32 | raw_data[258])
      .SetData0130(static_cast<int64_t>(raw_data[261]) << 32 | raw_data[260])
      .SetData0131(static_cast<int64_t>(raw_data[263]) << 32 | raw_data[262])
      .SetData0132(static_cast<int64_t>(raw_data[265]) << 32 | raw_data[264])
      .SetData0133(static_cast<int64_t>(raw_data[267]) << 32 | raw_data[266])
      .SetData0134(static_cast<int64_t>(raw_data[269]) << 32 | raw_data[268])
      .SetData0135(static_cast<int64_t>(raw_data[271]) << 32 | raw_data[270])
      .SetData0136(static_cast<int64_t>(raw_data[273]) << 32 | raw_data[272])
      .SetData0137(static_cast<int64_t>(raw_data[275]) << 32 | raw_data[274])
      .SetData0138(static_cast<int64_t>(raw_data[277]) << 32 | raw_data[276])
      .SetData0139(static_cast<int64_t>(raw_data[279]) << 32 | raw_data[278])
      .SetData0140(static_cast<int64_t>(raw_data[281]) << 32 | raw_data[280])
      .SetData0141(static_cast<int64_t>(raw_data[283]) << 32 | raw_data[282])
      .SetData0142(static_cast<int64_t>(raw_data[285]) << 32 | raw_data[284])
      .SetData0143(static_cast<int64_t>(raw_data[287]) << 32 | raw_data[286])
      .SetData0144(static_cast<int64_t>(raw_data[289]) << 32 | raw_data[288])
      .SetData0145(static_cast<int64_t>(raw_data[291]) << 32 | raw_data[290])
      .SetData0146(static_cast<int64_t>(raw_data[293]) << 32 | raw_data[292])
      .SetData0147(static_cast<int64_t>(raw_data[295]) << 32 | raw_data[294])
      .SetData0148(static_cast<int64_t>(raw_data[297]) << 32 | raw_data[296])
      .SetData0149(static_cast<int64_t>(raw_data[299]) << 32 | raw_data[298])
      .SetData0150(static_cast<int64_t>(raw_data[301]) << 32 | raw_data[300])
      .SetData0151(static_cast<int64_t>(raw_data[303]) << 32 | raw_data[302])
      .SetData0152(static_cast<int64_t>(raw_data[305]) << 32 | raw_data[304])
      .SetData0153(static_cast<int64_t>(raw_data[307]) << 32 | raw_data[306])
      .SetData0154(static_cast<int64_t>(raw_data[309]) << 32 | raw_data[308])
      .SetData0155(static_cast<int64_t>(raw_data[311]) << 32 | raw_data[310])
      .SetData0156(static_cast<int64_t>(raw_data[313]) << 32 | raw_data[312])
      .SetData0157(static_cast<int64_t>(raw_data[315]) << 32 | raw_data[314])
      .SetData0158(static_cast<int64_t>(raw_data[317]) << 32 | raw_data[316])
      .SetData0159(static_cast<int64_t>(raw_data[319]) << 32 | raw_data[318])
      .SetData0160(static_cast<int64_t>(raw_data[321]) << 32 | raw_data[320])
      .SetData0161(static_cast<int64_t>(raw_data[323]) << 32 | raw_data[322])
      .SetData0162(static_cast<int64_t>(raw_data[325]) << 32 | raw_data[324])
      .SetData0163(static_cast<int64_t>(raw_data[327]) << 32 | raw_data[326])
      .SetData0164(static_cast<int64_t>(raw_data[329]) << 32 | raw_data[328])
      .SetData0165(static_cast<int64_t>(raw_data[331]) << 32 | raw_data[330])
      .SetData0166(static_cast<int64_t>(raw_data[333]) << 32 | raw_data[332])
      .SetData0167(static_cast<int64_t>(raw_data[335]) << 32 | raw_data[334])
      .SetData0168(static_cast<int64_t>(raw_data[337]) << 32 | raw_data[336])
      .SetData0169(static_cast<int64_t>(raw_data[339]) << 32 | raw_data[338])
      .SetData0170(static_cast<int64_t>(raw_data[341]) << 32 | raw_data[340])
      .SetData0171(static_cast<int64_t>(raw_data[343]) << 32 | raw_data[342])
      .SetData0172(static_cast<int64_t>(raw_data[345]) << 32 | raw_data[344])
      .SetData0173(static_cast<int64_t>(raw_data[347]) << 32 | raw_data[346])
      .SetData0174(static_cast<int64_t>(raw_data[349]) << 32 | raw_data[348])
      .SetData0175(static_cast<int64_t>(raw_data[351]) << 32 | raw_data[350])
      .SetData0176(static_cast<int64_t>(raw_data[353]) << 32 | raw_data[352])
      .SetData0177(static_cast<int64_t>(raw_data[355]) << 32 | raw_data[354])
      .SetData0178(static_cast<int64_t>(raw_data[357]) << 32 | raw_data[356])
      .SetData0179(static_cast<int64_t>(raw_data[359]) << 32 | raw_data[358])
      .SetData0180(static_cast<int64_t>(raw_data[361]) << 32 | raw_data[360])
      .SetData0181(static_cast<int64_t>(raw_data[363]) << 32 | raw_data[362])
      .SetData0182(static_cast<int64_t>(raw_data[365]) << 32 | raw_data[364])
      .SetData0183(static_cast<int64_t>(raw_data[367]) << 32 | raw_data[366])
      .SetData0184(static_cast<int64_t>(raw_data[369]) << 32 | raw_data[368])
      .SetData0185(static_cast<int64_t>(raw_data[371]) << 32 | raw_data[370])
      .SetData0186(static_cast<int64_t>(raw_data[373]) << 32 | raw_data[372])
      .SetData0187(static_cast<int64_t>(raw_data[375]) << 32 | raw_data[374])
      .SetData0188(static_cast<int64_t>(raw_data[377]) << 32 | raw_data[376])
      .SetData0189(static_cast<int64_t>(raw_data[379]) << 32 | raw_data[378])
      .SetData0190(static_cast<int64_t>(raw_data[381]) << 32 | raw_data[380])
      .SetData0191(static_cast<int64_t>(raw_data[383]) << 32 | raw_data[382])
      .SetData0192(static_cast<int64_t>(raw_data[385]) << 32 | raw_data[384])
      .SetData0193(static_cast<int64_t>(raw_data[387]) << 32 | raw_data[386])
      .SetData0194(static_cast<int64_t>(raw_data[389]) << 32 | raw_data[388])
      .SetData0195(static_cast<int64_t>(raw_data[391]) << 32 | raw_data[390])
      .SetData0196(static_cast<int64_t>(raw_data[393]) << 32 | raw_data[392])
      .SetData0197(static_cast<int64_t>(raw_data[395]) << 32 | raw_data[394])
      .SetData0198(static_cast<int64_t>(raw_data[397]) << 32 | raw_data[396])
      .SetData0199(static_cast<int64_t>(raw_data[399]) << 32 | raw_data[398])
      .SetData0200(static_cast<int64_t>(raw_data[401]) << 32 | raw_data[400])
      .SetData0201(static_cast<int64_t>(raw_data[403]) << 32 | raw_data[402])
      .SetData0202(static_cast<int64_t>(raw_data[405]) << 32 | raw_data[404])
      .SetData0203(static_cast<int64_t>(raw_data[407]) << 32 | raw_data[406])
      .SetData0204(static_cast<int64_t>(raw_data[409]) << 32 | raw_data[408])
      .SetData0205(static_cast<int64_t>(raw_data[411]) << 32 | raw_data[410])
      .SetData0206(static_cast<int64_t>(raw_data[413]) << 32 | raw_data[412])
      .SetData0207(static_cast<int64_t>(raw_data[415]) << 32 | raw_data[414])
      .SetData0208(static_cast<int64_t>(raw_data[417]) << 32 | raw_data[416])
      .SetData0209(static_cast<int64_t>(raw_data[419]) << 32 | raw_data[418])
      .SetData0210(static_cast<int64_t>(raw_data[421]) << 32 | raw_data[420])
      .SetData0211(static_cast<int64_t>(raw_data[423]) << 32 | raw_data[422])
      .SetData0212(static_cast<int64_t>(raw_data[425]) << 32 | raw_data[424])
      .SetData0213(static_cast<int64_t>(raw_data[427]) << 32 | raw_data[426])
      .SetData0214(static_cast<int64_t>(raw_data[429]) << 32 | raw_data[428])
      .SetData0215(static_cast<int64_t>(raw_data[431]) << 32 | raw_data[430])
      .SetData0216(static_cast<int64_t>(raw_data[433]) << 32 | raw_data[432])
      .SetData0217(static_cast<int64_t>(raw_data[435]) << 32 | raw_data[434])
      .SetData0218(static_cast<int64_t>(raw_data[437]) << 32 | raw_data[436])
      .SetData0219(static_cast<int64_t>(raw_data[439]) << 32 | raw_data[438])
      .SetData0220(static_cast<int64_t>(raw_data[441]) << 32 | raw_data[440])
      .SetData0221(static_cast<int64_t>(raw_data[443]) << 32 | raw_data[442])
      .SetData0222(static_cast<int64_t>(raw_data[445]) << 32 | raw_data[444])
      .SetData0223(static_cast<int64_t>(raw_data[447]) << 32 | raw_data[446])
      .SetData0224(static_cast<int64_t>(raw_data[449]) << 32 | raw_data[448])
      .SetData0225(static_cast<int64_t>(raw_data[451]) << 32 | raw_data[450])
      .SetData0226(static_cast<int64_t>(raw_data[453]) << 32 | raw_data[452])
      .SetData0227(static_cast<int64_t>(raw_data[455]) << 32 | raw_data[454])
      .SetData0228(static_cast<int64_t>(raw_data[457]) << 32 | raw_data[456])
      .SetData0229(static_cast<int64_t>(raw_data[459]) << 32 | raw_data[458])
      .SetData0230(static_cast<int64_t>(raw_data[461]) << 32 | raw_data[460])
      .SetData0231(static_cast<int64_t>(raw_data[463]) << 32 | raw_data[462])
      .SetData0232(static_cast<int64_t>(raw_data[465]) << 32 | raw_data[464])
      .SetData0233(static_cast<int64_t>(raw_data[467]) << 32 | raw_data[466])
      .SetData0234(static_cast<int64_t>(raw_data[469]) << 32 | raw_data[468])
      .SetData0235(static_cast<int64_t>(raw_data[471]) << 32 | raw_data[470])
      .SetData0236(static_cast<int64_t>(raw_data[473]) << 32 | raw_data[472])
      .SetData0237(static_cast<int64_t>(raw_data[475]) << 32 | raw_data[474])
      .SetData0238(static_cast<int64_t>(raw_data[477]) << 32 | raw_data[476])
      .SetData0239(static_cast<int64_t>(raw_data[479]) << 32 | raw_data[478])
      .SetData0240(static_cast<int64_t>(raw_data[481]) << 32 | raw_data[480])
      .SetData0241(static_cast<int64_t>(raw_data[483]) << 32 | raw_data[482])
      .SetData0242(static_cast<int64_t>(raw_data[485]) << 32 | raw_data[484])
      .SetData0243(static_cast<int64_t>(raw_data[487]) << 32 | raw_data[486])
      .SetData0244(static_cast<int64_t>(raw_data[489]) << 32 | raw_data[488])
      .SetData0245(static_cast<int64_t>(raw_data[491]) << 32 | raw_data[490])
      .SetData0246(static_cast<int64_t>(raw_data[493]) << 32 | raw_data[492])
      .SetData0247(static_cast<int64_t>(raw_data[495]) << 32 | raw_data[494])
      .SetData0248(static_cast<int64_t>(raw_data[497]) << 32 | raw_data[496])
      .SetData0249(static_cast<int64_t>(raw_data[499]) << 32 | raw_data[498])
      .SetData0250(static_cast<int64_t>(raw_data[501]) << 32 | raw_data[500])
      .SetData0251(static_cast<int64_t>(raw_data[503]) << 32 | raw_data[502])
      .SetData0252(static_cast<int64_t>(raw_data[505]) << 32 | raw_data[504])
      .SetData0253(static_cast<int64_t>(raw_data[507]) << 32 | raw_data[506])
      .SetData0254(static_cast<int64_t>(raw_data[509]) << 32 | raw_data[508])
      .SetData0255(static_cast<int64_t>(raw_data[511]) << 32 | raw_data[510])
      .SetData0256(static_cast<int64_t>(raw_data[513]) << 32 | raw_data[512])
      .SetData0257(static_cast<int64_t>(raw_data[515]) << 32 | raw_data[514])
      .SetData0258(static_cast<int64_t>(raw_data[517]) << 32 | raw_data[516])
      .SetData0259(static_cast<int64_t>(raw_data[519]) << 32 | raw_data[518])
      .SetData0260(static_cast<int64_t>(raw_data[521]) << 32 | raw_data[520])
      .SetData0261(static_cast<int64_t>(raw_data[523]) << 32 | raw_data[522])
      .SetData0262(static_cast<int64_t>(raw_data[525]) << 32 | raw_data[524])
      .SetData0263(static_cast<int64_t>(raw_data[527]) << 32 | raw_data[526])
      .SetData0264(static_cast<int64_t>(raw_data[529]) << 32 | raw_data[528])
      .SetData0265(static_cast<int64_t>(raw_data[531]) << 32 | raw_data[530])
      .SetData0266(static_cast<int64_t>(raw_data[533]) << 32 | raw_data[532])
      .SetData0267(static_cast<int64_t>(raw_data[535]) << 32 | raw_data[534])
      .SetData0268(static_cast<int64_t>(raw_data[537]) << 32 | raw_data[536])
      .SetData0269(static_cast<int64_t>(raw_data[539]) << 32 | raw_data[538])
      .SetData0270(static_cast<int64_t>(raw_data[541]) << 32 | raw_data[540])
      .SetData0271(static_cast<int64_t>(raw_data[543]) << 32 | raw_data[542])
      .SetData0272(static_cast<int64_t>(raw_data[545]) << 32 | raw_data[544])
      .SetData0273(static_cast<int64_t>(raw_data[547]) << 32 | raw_data[546])
      .SetData0274(static_cast<int64_t>(raw_data[549]) << 32 | raw_data[548])
      .SetData0275(static_cast<int64_t>(raw_data[551]) << 32 | raw_data[550])
      .SetData0276(static_cast<int64_t>(raw_data[553]) << 32 | raw_data[552])
      .SetData0277(static_cast<int64_t>(raw_data[555]) << 32 | raw_data[554])
      .SetData0278(static_cast<int64_t>(raw_data[557]) << 32 | raw_data[556])
      .SetData0279(static_cast<int64_t>(raw_data[559]) << 32 | raw_data[558])
      .SetData0280(static_cast<int64_t>(raw_data[561]) << 32 | raw_data[560])
      .SetData0281(static_cast<int64_t>(raw_data[563]) << 32 | raw_data[562])
      .SetData0282(static_cast<int64_t>(raw_data[565]) << 32 | raw_data[564])
      .SetData0283(static_cast<int64_t>(raw_data[567]) << 32 | raw_data[566])
      .SetData0284(static_cast<int64_t>(raw_data[569]) << 32 | raw_data[568])
      .SetData0285(static_cast<int64_t>(raw_data[571]) << 32 | raw_data[570])
      .SetData0286(static_cast<int64_t>(raw_data[573]) << 32 | raw_data[572])
      .SetData0287(static_cast<int64_t>(raw_data[575]) << 32 | raw_data[574])
      .SetData0288(static_cast<int64_t>(raw_data[577]) << 32 | raw_data[576])
      .SetData0289(static_cast<int64_t>(raw_data[579]) << 32 | raw_data[578])
      .SetData0290(static_cast<int64_t>(raw_data[581]) << 32 | raw_data[580])
      .SetData0291(static_cast<int64_t>(raw_data[583]) << 32 | raw_data[582])
      .SetData0292(static_cast<int64_t>(raw_data[585]) << 32 | raw_data[584])
      .SetData0293(static_cast<int64_t>(raw_data[587]) << 32 | raw_data[586])
      .SetData0294(static_cast<int64_t>(raw_data[589]) << 32 | raw_data[588])
      .SetData0295(static_cast<int64_t>(raw_data[591]) << 32 | raw_data[590])
      .SetData0296(static_cast<int64_t>(raw_data[593]) << 32 | raw_data[592])
      .SetData0297(static_cast<int64_t>(raw_data[595]) << 32 | raw_data[594])
      .SetData0298(static_cast<int64_t>(raw_data[597]) << 32 | raw_data[596])
      .SetData0299(static_cast<int64_t>(raw_data[599]) << 32 | raw_data[598])
      .SetData0300(static_cast<int64_t>(raw_data[601]) << 32 | raw_data[600])
      .SetData0301(static_cast<int64_t>(raw_data[603]) << 32 | raw_data[602])
      .SetData0302(static_cast<int64_t>(raw_data[605]) << 32 | raw_data[604])
      .SetData0303(static_cast<int64_t>(raw_data[607]) << 32 | raw_data[606])
      .SetData0304(static_cast<int64_t>(raw_data[609]) << 32 | raw_data[608])
      .SetData0305(static_cast<int64_t>(raw_data[611]) << 32 | raw_data[610])
      .SetData0306(static_cast<int64_t>(raw_data[613]) << 32 | raw_data[612])
      .SetData0307(static_cast<int64_t>(raw_data[615]) << 32 | raw_data[614])
      .SetData0308(static_cast<int64_t>(raw_data[617]) << 32 | raw_data[616])
      .SetData0309(static_cast<int64_t>(raw_data[619]) << 32 | raw_data[618])
      .SetData0310(static_cast<int64_t>(raw_data[621]) << 32 | raw_data[620])
      .SetData0311(static_cast<int64_t>(raw_data[623]) << 32 | raw_data[622])
      .SetData0312(static_cast<int64_t>(raw_data[625]) << 32 | raw_data[624])
      .SetData0313(static_cast<int64_t>(raw_data[627]) << 32 | raw_data[626])
      .SetData0314(static_cast<int64_t>(raw_data[629]) << 32 | raw_data[628])
      .SetData0315(static_cast<int64_t>(raw_data[631]) << 32 | raw_data[630])
      .SetData0316(static_cast<int64_t>(raw_data[633]) << 32 | raw_data[632])
      .SetData0317(static_cast<int64_t>(raw_data[635]) << 32 | raw_data[634])
      .SetData0318(static_cast<int64_t>(raw_data[637]) << 32 | raw_data[636])
      .SetData0319(static_cast<int64_t>(raw_data[639]) << 32 | raw_data[638])
      .SetData0320(static_cast<int64_t>(raw_data[641]) << 32 | raw_data[640])
      .SetData0321(static_cast<int64_t>(raw_data[643]) << 32 | raw_data[642])
      .SetData0322(static_cast<int64_t>(raw_data[645]) << 32 | raw_data[644])
      .SetData0323(static_cast<int64_t>(raw_data[647]) << 32 | raw_data[646])
      .SetData0324(static_cast<int64_t>(raw_data[649]) << 32 | raw_data[648])
      .SetData0325(static_cast<int64_t>(raw_data[651]) << 32 | raw_data[650])
      .SetData0326(static_cast<int64_t>(raw_data[653]) << 32 | raw_data[652])
      .SetData0327(static_cast<int64_t>(raw_data[655]) << 32 | raw_data[654])
      .SetData0328(static_cast<int64_t>(raw_data[657]) << 32 | raw_data[656])
      .SetData0329(static_cast<int64_t>(raw_data[659]) << 32 | raw_data[658])
      .SetData0330(static_cast<int64_t>(raw_data[661]) << 32 | raw_data[660])
      .SetData0331(static_cast<int64_t>(raw_data[663]) << 32 | raw_data[662])
      .SetData0332(static_cast<int64_t>(raw_data[665]) << 32 | raw_data[664])
      .SetData0333(static_cast<int64_t>(raw_data[667]) << 32 | raw_data[666])
      .SetData0334(static_cast<int64_t>(raw_data[669]) << 32 | raw_data[668])
      .SetData0335(static_cast<int64_t>(raw_data[671]) << 32 | raw_data[670])
      .SetData0336(static_cast<int64_t>(raw_data[673]) << 32 | raw_data[672])
      .SetData0337(static_cast<int64_t>(raw_data[675]) << 32 | raw_data[674])
      .SetData0338(static_cast<int64_t>(raw_data[677]) << 32 | raw_data[676])
      .SetData0339(static_cast<int64_t>(raw_data[679]) << 32 | raw_data[678])
      .SetData0340(static_cast<int64_t>(raw_data[681]) << 32 | raw_data[680])
      .SetData0341(static_cast<int64_t>(raw_data[683]) << 32 | raw_data[682])
      .SetData0342(static_cast<int64_t>(raw_data[685]) << 32 | raw_data[684])
      .SetData0343(static_cast<int64_t>(raw_data[687]) << 32 | raw_data[686])
      .SetData0344(static_cast<int64_t>(raw_data[689]) << 32 | raw_data[688])
      .SetData0345(static_cast<int64_t>(raw_data[691]) << 32 | raw_data[690])
      .SetData0346(static_cast<int64_t>(raw_data[693]) << 32 | raw_data[692])
      .SetData0347(static_cast<int64_t>(raw_data[695]) << 32 | raw_data[694])
      .SetData0348(static_cast<int64_t>(raw_data[697]) << 32 | raw_data[696])
      .SetData0349(static_cast<int64_t>(raw_data[699]) << 32 | raw_data[698])
      .SetData0350(static_cast<int64_t>(raw_data[701]) << 32 | raw_data[700])
      .SetData0351(static_cast<int64_t>(raw_data[703]) << 32 | raw_data[702])
      .SetData0352(static_cast<int64_t>(raw_data[705]) << 32 | raw_data[704])
      .SetData0353(static_cast<int64_t>(raw_data[707]) << 32 | raw_data[706])
      .SetData0354(static_cast<int64_t>(raw_data[709]) << 32 | raw_data[708])
      .SetData0355(static_cast<int64_t>(raw_data[711]) << 32 | raw_data[710])
      .SetData0356(static_cast<int64_t>(raw_data[713]) << 32 | raw_data[712])
      .SetData0357(static_cast<int64_t>(raw_data[715]) << 32 | raw_data[714])
      .SetData0358(static_cast<int64_t>(raw_data[717]) << 32 | raw_data[716])
      .SetData0359(static_cast<int64_t>(raw_data[719]) << 32 | raw_data[718])
      .SetData0360(static_cast<int64_t>(raw_data[721]) << 32 | raw_data[720])
      .SetData0361(static_cast<int64_t>(raw_data[723]) << 32 | raw_data[722])
      .SetData0362(static_cast<int64_t>(raw_data[725]) << 32 | raw_data[724])
      .SetData0363(static_cast<int64_t>(raw_data[727]) << 32 | raw_data[726])
      .SetData0364(static_cast<int64_t>(raw_data[729]) << 32 | raw_data[728])
      .SetData0365(static_cast<int64_t>(raw_data[731]) << 32 | raw_data[730])
      .SetData0366(static_cast<int64_t>(raw_data[733]) << 32 | raw_data[732])
      .SetData0367(static_cast<int64_t>(raw_data[735]) << 32 | raw_data[734])
      .SetData0368(static_cast<int64_t>(raw_data[737]) << 32 | raw_data[736])
      .SetData0369(static_cast<int64_t>(raw_data[739]) << 32 | raw_data[738])
      .SetData0370(static_cast<int64_t>(raw_data[741]) << 32 | raw_data[740])
      .SetData0371(static_cast<int64_t>(raw_data[743]) << 32 | raw_data[742])
      .SetData0372(static_cast<int64_t>(raw_data[745]) << 32 | raw_data[744])
      .SetData0373(static_cast<int64_t>(raw_data[747]) << 32 | raw_data[746])
      .SetData0374(static_cast<int64_t>(raw_data[749]) << 32 | raw_data[748])
      .SetData0375(static_cast<int64_t>(raw_data[751]) << 32 | raw_data[750])
      .SetData0376(static_cast<int64_t>(raw_data[753]) << 32 | raw_data[752])
      .SetData0377(static_cast<int64_t>(raw_data[755]) << 32 | raw_data[754])
      .SetData0378(static_cast<int64_t>(raw_data[757]) << 32 | raw_data[756])
      .SetData0379(static_cast<int64_t>(raw_data[759]) << 32 | raw_data[758])
      .SetData0380(static_cast<int64_t>(raw_data[761]) << 32 | raw_data[760])
      .SetData0381(static_cast<int64_t>(raw_data[763]) << 32 | raw_data[762])
      .SetData0382(static_cast<int64_t>(raw_data[765]) << 32 | raw_data[764])
      .SetData0383(static_cast<int64_t>(raw_data[767]) << 32 | raw_data[766])
      .SetData0384(static_cast<int64_t>(raw_data[769]) << 32 | raw_data[768])
      .SetData0385(static_cast<int64_t>(raw_data[771]) << 32 | raw_data[770])
      .SetData0386(static_cast<int64_t>(raw_data[773]) << 32 | raw_data[772])
      .SetData0387(static_cast<int64_t>(raw_data[775]) << 32 | raw_data[774])
      .SetData0388(static_cast<int64_t>(raw_data[777]) << 32 | raw_data[776])
      .SetData0389(static_cast<int64_t>(raw_data[779]) << 32 | raw_data[778])
      .SetData0390(static_cast<int64_t>(raw_data[781]) << 32 | raw_data[780])
      .SetData0391(static_cast<int64_t>(raw_data[783]) << 32 | raw_data[782])
      .SetData0392(static_cast<int64_t>(raw_data[785]) << 32 | raw_data[784])
      .SetData0393(static_cast<int64_t>(raw_data[787]) << 32 | raw_data[786])
      .SetData0394(static_cast<int64_t>(raw_data[789]) << 32 | raw_data[788])
      .SetData0395(static_cast<int64_t>(raw_data[791]) << 32 | raw_data[790])
      .SetData0396(static_cast<int64_t>(raw_data[793]) << 32 | raw_data[792])
      .SetData0397(static_cast<int64_t>(raw_data[795]) << 32 | raw_data[794])
      .SetData0398(static_cast<int64_t>(raw_data[797]) << 32 | raw_data[796])
      .SetData0399(static_cast<int64_t>(raw_data[799]) << 32 | raw_data[798])
      .SetData0400(static_cast<int64_t>(raw_data[801]) << 32 | raw_data[800])
      .SetData0401(static_cast<int64_t>(raw_data[803]) << 32 | raw_data[802])
      .SetData0402(static_cast<int64_t>(raw_data[805]) << 32 | raw_data[804])
      .SetData0403(static_cast<int64_t>(raw_data[807]) << 32 | raw_data[806])
      .SetData0404(static_cast<int64_t>(raw_data[809]) << 32 | raw_data[808])
      .SetData0405(static_cast<int64_t>(raw_data[811]) << 32 | raw_data[810])
      .SetData0406(static_cast<int64_t>(raw_data[813]) << 32 | raw_data[812])
      .SetData0407(static_cast<int64_t>(raw_data[815]) << 32 | raw_data[814])
      .SetData0408(static_cast<int64_t>(raw_data[817]) << 32 | raw_data[816])
      .SetData0409(static_cast<int64_t>(raw_data[819]) << 32 | raw_data[818])
      .SetData0410(static_cast<int64_t>(raw_data[821]) << 32 | raw_data[820])
      .SetData0411(static_cast<int64_t>(raw_data[823]) << 32 | raw_data[822])
      .SetData0412(static_cast<int64_t>(raw_data[825]) << 32 | raw_data[824])
      .SetData0413(static_cast<int64_t>(raw_data[827]) << 32 | raw_data[826])
      .SetData0414(static_cast<int64_t>(raw_data[829]) << 32 | raw_data[828])
      .SetData0415(static_cast<int64_t>(raw_data[831]) << 32 | raw_data[830])
      .SetData0416(static_cast<int64_t>(raw_data[833]) << 32 | raw_data[832])
      .SetData0417(static_cast<int64_t>(raw_data[835]) << 32 | raw_data[834])
      .SetData0418(static_cast<int64_t>(raw_data[837]) << 32 | raw_data[836])
      .SetData0419(static_cast<int64_t>(raw_data[839]) << 32 | raw_data[838])
      .SetData0420(static_cast<int64_t>(raw_data[841]) << 32 | raw_data[840])
      .SetData0421(static_cast<int64_t>(raw_data[843]) << 32 | raw_data[842])
      .SetData0422(static_cast<int64_t>(raw_data[845]) << 32 | raw_data[844])
      .SetData0423(static_cast<int64_t>(raw_data[847]) << 32 | raw_data[846])
      .SetData0424(static_cast<int64_t>(raw_data[849]) << 32 | raw_data[848])
      .SetData0425(static_cast<int64_t>(raw_data[851]) << 32 | raw_data[850])
      .SetData0426(static_cast<int64_t>(raw_data[853]) << 32 | raw_data[852])
      .SetData0427(static_cast<int64_t>(raw_data[855]) << 32 | raw_data[854])
      .SetData0428(static_cast<int64_t>(raw_data[857]) << 32 | raw_data[856])
      .SetData0429(static_cast<int64_t>(raw_data[859]) << 32 | raw_data[858])
      .SetData0430(static_cast<int64_t>(raw_data[861]) << 32 | raw_data[860])
      .SetData0431(static_cast<int64_t>(raw_data[863]) << 32 | raw_data[862])
      .SetData0432(static_cast<int64_t>(raw_data[865]) << 32 | raw_data[864])
      .SetData0433(static_cast<int64_t>(raw_data[867]) << 32 | raw_data[866])
      .SetData0434(static_cast<int64_t>(raw_data[869]) << 32 | raw_data[868])
      .SetData0435(static_cast<int64_t>(raw_data[871]) << 32 | raw_data[870])
      .SetData0436(static_cast<int64_t>(raw_data[873]) << 32 | raw_data[872])
      .SetData0437(static_cast<int64_t>(raw_data[875]) << 32 | raw_data[874])
      .SetData0438(static_cast<int64_t>(raw_data[877]) << 32 | raw_data[876])
      .SetData0439(static_cast<int64_t>(raw_data[879]) << 32 | raw_data[878])
      .SetData0440(static_cast<int64_t>(raw_data[881]) << 32 | raw_data[880])
      .SetData0441(static_cast<int64_t>(raw_data[883]) << 32 | raw_data[882])
      .SetData0442(static_cast<int64_t>(raw_data[885]) << 32 | raw_data[884])
      .SetData0443(static_cast<int64_t>(raw_data[887]) << 32 | raw_data[886])
      .SetData0444(static_cast<int64_t>(raw_data[889]) << 32 | raw_data[888])
      .SetData0445(static_cast<int64_t>(raw_data[891]) << 32 | raw_data[890])
      .SetData0446(static_cast<int64_t>(raw_data[893]) << 32 | raw_data[892])
      .SetData0447(static_cast<int64_t>(raw_data[895]) << 32 | raw_data[894])
      .SetData0448(static_cast<int64_t>(raw_data[897]) << 32 | raw_data[896])
      .SetData0449(static_cast<int64_t>(raw_data[899]) << 32 | raw_data[898])
      .SetData0450(static_cast<int64_t>(raw_data[901]) << 32 | raw_data[900])
      .SetData0451(static_cast<int64_t>(raw_data[903]) << 32 | raw_data[902])
      .SetData0452(static_cast<int64_t>(raw_data[905]) << 32 | raw_data[904])
      .SetData0453(static_cast<int64_t>(raw_data[907]) << 32 | raw_data[906])
      .SetData0454(static_cast<int64_t>(raw_data[909]) << 32 | raw_data[908])
      .SetData0455(static_cast<int64_t>(raw_data[911]) << 32 | raw_data[910])
      .SetData0456(static_cast<int64_t>(raw_data[913]) << 32 | raw_data[912])
      .SetData0457(static_cast<int64_t>(raw_data[915]) << 32 | raw_data[914])
      .SetData0458(static_cast<int64_t>(raw_data[917]) << 32 | raw_data[916])
      .SetData0459(static_cast<int64_t>(raw_data[919]) << 32 | raw_data[918])
      .SetData0460(static_cast<int64_t>(raw_data[921]) << 32 | raw_data[920])
      .SetData0461(static_cast<int64_t>(raw_data[923]) << 32 | raw_data[922])
      .SetData0462(static_cast<int64_t>(raw_data[925]) << 32 | raw_data[924])
      .SetData0463(static_cast<int64_t>(raw_data[927]) << 32 | raw_data[926])
      .SetData0464(static_cast<int64_t>(raw_data[929]) << 32 | raw_data[928])
      .SetData0465(static_cast<int64_t>(raw_data[931]) << 32 | raw_data[930])
      .SetData0466(static_cast<int64_t>(raw_data[933]) << 32 | raw_data[932])
      .SetData0467(static_cast<int64_t>(raw_data[935]) << 32 | raw_data[934])
      .SetData0468(static_cast<int64_t>(raw_data[937]) << 32 | raw_data[936])
      .SetData0469(static_cast<int64_t>(raw_data[939]) << 32 | raw_data[938])
      .SetData0470(static_cast<int64_t>(raw_data[941]) << 32 | raw_data[940])
      .SetData0471(static_cast<int64_t>(raw_data[943]) << 32 | raw_data[942])
      .SetData0472(static_cast<int64_t>(raw_data[945]) << 32 | raw_data[944])
      .SetData0473(static_cast<int64_t>(raw_data[947]) << 32 | raw_data[946])
      .SetData0474(static_cast<int64_t>(raw_data[949]) << 32 | raw_data[948])
      .SetData0475(static_cast<int64_t>(raw_data[951]) << 32 | raw_data[950])
      .SetData0476(static_cast<int64_t>(raw_data[953]) << 32 | raw_data[952])
      .SetData0477(static_cast<int64_t>(raw_data[955]) << 32 | raw_data[954])
      .SetData0478(static_cast<int64_t>(raw_data[957]) << 32 | raw_data[956])
      .SetData0479(static_cast<int64_t>(raw_data[959]) << 32 | raw_data[958])
      .SetData0480(static_cast<int64_t>(raw_data[961]) << 32 | raw_data[960])
      .SetData0481(static_cast<int64_t>(raw_data[963]) << 32 | raw_data[962])
      .SetData0482(static_cast<int64_t>(raw_data[965]) << 32 | raw_data[964])
      .SetData0483(static_cast<int64_t>(raw_data[967]) << 32 | raw_data[966])
      .SetData0484(static_cast<int64_t>(raw_data[969]) << 32 | raw_data[968])
      .SetData0485(static_cast<int64_t>(raw_data[971]) << 32 | raw_data[970])
      .SetData0486(static_cast<int64_t>(raw_data[973]) << 32 | raw_data[972])
      .SetData0487(static_cast<int64_t>(raw_data[975]) << 32 | raw_data[974])
      .SetData0488(static_cast<int64_t>(raw_data[977]) << 32 | raw_data[976])
      .SetData0489(static_cast<int64_t>(raw_data[979]) << 32 | raw_data[978])
      .SetData0490(static_cast<int64_t>(raw_data[981]) << 32 | raw_data[980])
      .SetData0491(static_cast<int64_t>(raw_data[983]) << 32 | raw_data[982])
      .SetData0492(static_cast<int64_t>(raw_data[985]) << 32 | raw_data[984])
      .SetData0493(static_cast<int64_t>(raw_data[987]) << 32 | raw_data[986])
      .SetData0494(static_cast<int64_t>(raw_data[989]) << 32 | raw_data[988])
      .SetData0495(static_cast<int64_t>(raw_data[991]) << 32 | raw_data[990])
      .SetData0496(static_cast<int64_t>(raw_data[993]) << 32 | raw_data[992])
      .SetData0497(static_cast<int64_t>(raw_data[995]) << 32 | raw_data[994])
      .SetData0498(static_cast<int64_t>(raw_data[997]) << 32 | raw_data[996])
      .SetData0499(static_cast<int64_t>(raw_data[999]) << 32 | raw_data[998])
      .SetData0500(static_cast<int64_t>(raw_data[1001]) << 32 | raw_data[1000])
      .SetData0501(static_cast<int64_t>(raw_data[1003]) << 32 | raw_data[1002])
      .SetData0502(static_cast<int64_t>(raw_data[1005]) << 32 | raw_data[1004])
      .SetData0503(static_cast<int64_t>(raw_data[1007]) << 32 | raw_data[1006])
      .SetData0504(static_cast<int64_t>(raw_data[1009]) << 32 | raw_data[1008])
      .SetData0505(static_cast<int64_t>(raw_data[1011]) << 32 | raw_data[1010])
      .SetData0506(static_cast<int64_t>(raw_data[1013]) << 32 | raw_data[1012])
      .SetData0507(static_cast<int64_t>(raw_data[1015]) << 32 | raw_data[1014])
      .SetData0508(static_cast<int64_t>(raw_data[1017]) << 32 | raw_data[1016])
      .SetData0509(static_cast<int64_t>(raw_data[1019]) << 32 | raw_data[1018])
      .SetData0510(static_cast<int64_t>(raw_data[1021]) << 32 | raw_data[1020])
      .SetData0511(static_cast<int64_t>(raw_data[1023]) << 32 | raw_data[1022])
      .Record(ukm_recorder);
  return true;
}

void V8CompileHints::AddNoise(unsigned* data) {
  // Add differential privacy noise:
  // With noise / 2 probability, the bit will be 0.
  // With noise / 2 probability, the bit will be 1.
  // With 1 - noise probability, the bit will keep its real value.

  // This is equivalent with flipping each bit with noise / 2 probability:
  // If the bit is 1 with probability p, the resulting bit is 1 with
  // probability...

  // Differential privacy: noise / 2 + p * (1 - noise)
  //                       = p - p * noise + noise / 2.

  // Bit flipping: noise / 2 * (1 - p) + (1 - noise / 2) * p
  //               = noise / 2 - p * noise / 2 + p - p * noise / 2
  //               = p - p * noise + noise / 2.

  // Which bits should be flipped.
  unsigned mask = 0;

  constexpr int bitsInUnsigned = sizeof(unsigned) * 8;
  double noiseLevel = features::kProduceCompileHintsNoiseLevel.Get();
  for (int i = 0; i < bitsInUnsigned; ++i) {
    if (i > 0) {
      mask <<= 1;
    }
    double random = base::RandDouble();
    if (random < noiseLevel / 2) {
      // Change this bit.
      mask |= 1;
    }
  }

  *data = *data ^ mask;
}

}  // namespace blink

#endif  // BUILDFLAG(ENABLE_V8_COMPILE_HINTS)
