// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints.h"

#include "base/hash/hash.h"
#include "base/rand_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/bloom_filter.h"

#include <limits>

namespace blink {

namespace {
constexpr int kBloomFilterInt32Count = 512;
}

std::atomic<bool> V8CompileHints::data_generated_for_this_process_ = false;

void V8CompileHints::RecordScript(Frame* frame,
                                  ExecutionContext* execution_context,
                                  const v8::Local<v8::Script> script,
                                  ScriptState* script_state) {
  if (state_ == State::kDataGenerationFinished) {
    // We've already generated data for this V8CompileHints. Don't record any
    // script compilations happening after it.
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

  ScheduleDataGenerationIfNeeded(frame, execution_context);
}

namespace {
void DelayedDataGenerationTask(Page* page,
                               ExecutionContext* execution_context) {
  page->GetV8CompileHints().GenerateData(execution_context);
}
}  // namespace

void V8CompileHints::ScheduleDataGenerationIfNeeded(
    Frame* frame,
    ExecutionContext* execution_context) {
  // We need to use the outermost main frame's ExecutionContext for retrieving
  // the UkmRecorder for sending the data. This means that if the main frame
  // doesn't run scripts, compile hints from the non-main frames won't be sent.
  // TODO(chromium:1406506): Relax that restriction.
  if (!frame->IsOutermostMainFrame()) {
    return;
  }

  DCHECK(state_ == State::kInitial ||
         state_ == State::kDataGenerationScheduled);
  if (state_ == State::kDataGenerationScheduled) {
    return;
  }

  state_ = State::kDataGenerationScheduled;

  // Schedule a task for moving the data to UKM. For now, we use a simple timer
  // instead of a more complicated "page loaded" event, but this should be good
  // enough for our purpose.

  auto delay =
      base::Milliseconds(features::kProduceCompileHintsOnIdleDelayParam.Get());

  execution_context->GetTaskRunner(TaskType::kIdleTask)
      ->PostDelayedTask(FROM_HERE,
                        WTF::BindOnce(&DelayedDataGenerationTask,
                                      WrapPersistent(frame->GetPage()),
                                      WrapPersistent(execution_context)),
                        delay);
}

void V8CompileHints::GenerateData(ExecutionContext* execution_context) {
  if (state_ == State::kDataGenerationFinished) {
    // This only happens when: 1) some other Page generated data 2)
    // this V8CompileHints object got notified of a script 3) it realized that
    // some other Page has already generated data 4) the data generation task
    // which was already scheduled ran.
    DCHECK(data_generated_for_this_process_);
    return;
  }

  // Stop logging script executions for this page.
  state_ = State::kDataGenerationFinished;

  if (!data_generated_for_this_process_) {
    data_generated_for_this_process_ = SendDataToUkm(execution_context);
  }

  ClearData();
}

void V8CompileHints::ClearData() {
  scripts_.clear();
  script_name_hashes_.clear();
}

bool V8CompileHints::SendDataToUkm(ExecutionContext* execution_context) {
  v8::Isolate* isolate = execution_context->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  int total_funcs = 0;

  DCHECK_EQ(scripts_.size(), script_name_hashes_.size());

  // Create a Bloom filter w/ 14 key bits. This results in a Bloom filter
  // containing 2 ^ 14 bits, which equals to 256 64-bit ints.
  constexpr int kBloomFilterKeySize = 14;
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
  ukm::builders::V8CompileHints_Version1(execution_context->UkmSourceID())
      .SetData000(static_cast<int64_t>(raw_data[1]) << 32 | raw_data[0])
      .SetData001(static_cast<int64_t>(raw_data[3]) << 32 | raw_data[2])
      .SetData002(static_cast<int64_t>(raw_data[5]) << 32 | raw_data[4])
      .SetData003(static_cast<int64_t>(raw_data[7]) << 32 | raw_data[6])
      .SetData004(static_cast<int64_t>(raw_data[9]) << 32 | raw_data[8])
      .SetData005(static_cast<int64_t>(raw_data[11]) << 32 | raw_data[10])
      .SetData006(static_cast<int64_t>(raw_data[13]) << 32 | raw_data[12])
      .SetData007(static_cast<int64_t>(raw_data[15]) << 32 | raw_data[14])
      .SetData008(static_cast<int64_t>(raw_data[17]) << 32 | raw_data[16])
      .SetData009(static_cast<int64_t>(raw_data[19]) << 32 | raw_data[18])
      .SetData010(static_cast<int64_t>(raw_data[21]) << 32 | raw_data[20])
      .SetData011(static_cast<int64_t>(raw_data[23]) << 32 | raw_data[22])
      .SetData012(static_cast<int64_t>(raw_data[25]) << 32 | raw_data[24])
      .SetData013(static_cast<int64_t>(raw_data[27]) << 32 | raw_data[26])
      .SetData014(static_cast<int64_t>(raw_data[29]) << 32 | raw_data[28])
      .SetData015(static_cast<int64_t>(raw_data[31]) << 32 | raw_data[30])
      .SetData016(static_cast<int64_t>(raw_data[33]) << 32 | raw_data[32])
      .SetData017(static_cast<int64_t>(raw_data[35]) << 32 | raw_data[34])
      .SetData018(static_cast<int64_t>(raw_data[37]) << 32 | raw_data[36])
      .SetData019(static_cast<int64_t>(raw_data[39]) << 32 | raw_data[38])
      .SetData020(static_cast<int64_t>(raw_data[41]) << 32 | raw_data[40])
      .SetData021(static_cast<int64_t>(raw_data[43]) << 32 | raw_data[42])
      .SetData022(static_cast<int64_t>(raw_data[45]) << 32 | raw_data[44])
      .SetData023(static_cast<int64_t>(raw_data[47]) << 32 | raw_data[46])
      .SetData024(static_cast<int64_t>(raw_data[49]) << 32 | raw_data[48])
      .SetData025(static_cast<int64_t>(raw_data[51]) << 32 | raw_data[50])
      .SetData026(static_cast<int64_t>(raw_data[53]) << 32 | raw_data[52])
      .SetData027(static_cast<int64_t>(raw_data[55]) << 32 | raw_data[54])
      .SetData028(static_cast<int64_t>(raw_data[57]) << 32 | raw_data[56])
      .SetData029(static_cast<int64_t>(raw_data[59]) << 32 | raw_data[58])
      .SetData030(static_cast<int64_t>(raw_data[61]) << 32 | raw_data[60])
      .SetData031(static_cast<int64_t>(raw_data[63]) << 32 | raw_data[62])
      .SetData032(static_cast<int64_t>(raw_data[65]) << 32 | raw_data[64])
      .SetData033(static_cast<int64_t>(raw_data[67]) << 32 | raw_data[66])
      .SetData034(static_cast<int64_t>(raw_data[69]) << 32 | raw_data[68])
      .SetData035(static_cast<int64_t>(raw_data[71]) << 32 | raw_data[70])
      .SetData036(static_cast<int64_t>(raw_data[73]) << 32 | raw_data[72])
      .SetData037(static_cast<int64_t>(raw_data[75]) << 32 | raw_data[74])
      .SetData038(static_cast<int64_t>(raw_data[77]) << 32 | raw_data[76])
      .SetData039(static_cast<int64_t>(raw_data[79]) << 32 | raw_data[78])
      .SetData040(static_cast<int64_t>(raw_data[81]) << 32 | raw_data[80])
      .SetData041(static_cast<int64_t>(raw_data[83]) << 32 | raw_data[82])
      .SetData042(static_cast<int64_t>(raw_data[85]) << 32 | raw_data[84])
      .SetData043(static_cast<int64_t>(raw_data[87]) << 32 | raw_data[86])
      .SetData044(static_cast<int64_t>(raw_data[89]) << 32 | raw_data[88])
      .SetData045(static_cast<int64_t>(raw_data[91]) << 32 | raw_data[90])
      .SetData046(static_cast<int64_t>(raw_data[93]) << 32 | raw_data[92])
      .SetData047(static_cast<int64_t>(raw_data[95]) << 32 | raw_data[94])
      .SetData048(static_cast<int64_t>(raw_data[97]) << 32 | raw_data[96])
      .SetData049(static_cast<int64_t>(raw_data[99]) << 32 | raw_data[98])
      .SetData050(static_cast<int64_t>(raw_data[101]) << 32 | raw_data[100])
      .SetData051(static_cast<int64_t>(raw_data[103]) << 32 | raw_data[102])
      .SetData052(static_cast<int64_t>(raw_data[105]) << 32 | raw_data[104])
      .SetData053(static_cast<int64_t>(raw_data[107]) << 32 | raw_data[106])
      .SetData054(static_cast<int64_t>(raw_data[109]) << 32 | raw_data[108])
      .SetData055(static_cast<int64_t>(raw_data[111]) << 32 | raw_data[110])
      .SetData056(static_cast<int64_t>(raw_data[113]) << 32 | raw_data[112])
      .SetData057(static_cast<int64_t>(raw_data[115]) << 32 | raw_data[114])
      .SetData058(static_cast<int64_t>(raw_data[117]) << 32 | raw_data[116])
      .SetData059(static_cast<int64_t>(raw_data[119]) << 32 | raw_data[118])
      .SetData060(static_cast<int64_t>(raw_data[121]) << 32 | raw_data[120])
      .SetData061(static_cast<int64_t>(raw_data[123]) << 32 | raw_data[122])
      .SetData062(static_cast<int64_t>(raw_data[125]) << 32 | raw_data[124])
      .SetData063(static_cast<int64_t>(raw_data[127]) << 32 | raw_data[126])
      .SetData064(static_cast<int64_t>(raw_data[129]) << 32 | raw_data[128])
      .SetData065(static_cast<int64_t>(raw_data[131]) << 32 | raw_data[130])
      .SetData066(static_cast<int64_t>(raw_data[133]) << 32 | raw_data[132])
      .SetData067(static_cast<int64_t>(raw_data[135]) << 32 | raw_data[134])
      .SetData068(static_cast<int64_t>(raw_data[137]) << 32 | raw_data[136])
      .SetData069(static_cast<int64_t>(raw_data[139]) << 32 | raw_data[138])
      .SetData070(static_cast<int64_t>(raw_data[141]) << 32 | raw_data[140])
      .SetData071(static_cast<int64_t>(raw_data[143]) << 32 | raw_data[142])
      .SetData072(static_cast<int64_t>(raw_data[145]) << 32 | raw_data[144])
      .SetData073(static_cast<int64_t>(raw_data[147]) << 32 | raw_data[146])
      .SetData074(static_cast<int64_t>(raw_data[149]) << 32 | raw_data[148])
      .SetData075(static_cast<int64_t>(raw_data[151]) << 32 | raw_data[150])
      .SetData076(static_cast<int64_t>(raw_data[153]) << 32 | raw_data[152])
      .SetData077(static_cast<int64_t>(raw_data[155]) << 32 | raw_data[154])
      .SetData078(static_cast<int64_t>(raw_data[157]) << 32 | raw_data[156])
      .SetData079(static_cast<int64_t>(raw_data[159]) << 32 | raw_data[158])
      .SetData080(static_cast<int64_t>(raw_data[161]) << 32 | raw_data[160])
      .SetData081(static_cast<int64_t>(raw_data[163]) << 32 | raw_data[162])
      .SetData082(static_cast<int64_t>(raw_data[165]) << 32 | raw_data[164])
      .SetData083(static_cast<int64_t>(raw_data[167]) << 32 | raw_data[166])
      .SetData084(static_cast<int64_t>(raw_data[169]) << 32 | raw_data[168])
      .SetData085(static_cast<int64_t>(raw_data[171]) << 32 | raw_data[170])
      .SetData086(static_cast<int64_t>(raw_data[173]) << 32 | raw_data[172])
      .SetData087(static_cast<int64_t>(raw_data[175]) << 32 | raw_data[174])
      .SetData088(static_cast<int64_t>(raw_data[177]) << 32 | raw_data[176])
      .SetData089(static_cast<int64_t>(raw_data[179]) << 32 | raw_data[178])
      .SetData090(static_cast<int64_t>(raw_data[181]) << 32 | raw_data[180])
      .SetData091(static_cast<int64_t>(raw_data[183]) << 32 | raw_data[182])
      .SetData092(static_cast<int64_t>(raw_data[185]) << 32 | raw_data[184])
      .SetData093(static_cast<int64_t>(raw_data[187]) << 32 | raw_data[186])
      .SetData094(static_cast<int64_t>(raw_data[189]) << 32 | raw_data[188])
      .SetData095(static_cast<int64_t>(raw_data[191]) << 32 | raw_data[190])
      .SetData096(static_cast<int64_t>(raw_data[193]) << 32 | raw_data[192])
      .SetData097(static_cast<int64_t>(raw_data[195]) << 32 | raw_data[194])
      .SetData098(static_cast<int64_t>(raw_data[197]) << 32 | raw_data[196])
      .SetData099(static_cast<int64_t>(raw_data[199]) << 32 | raw_data[198])
      .SetData100(static_cast<int64_t>(raw_data[201]) << 32 | raw_data[200])
      .SetData101(static_cast<int64_t>(raw_data[203]) << 32 | raw_data[202])
      .SetData102(static_cast<int64_t>(raw_data[205]) << 32 | raw_data[204])
      .SetData103(static_cast<int64_t>(raw_data[207]) << 32 | raw_data[206])
      .SetData104(static_cast<int64_t>(raw_data[209]) << 32 | raw_data[208])
      .SetData105(static_cast<int64_t>(raw_data[211]) << 32 | raw_data[210])
      .SetData106(static_cast<int64_t>(raw_data[213]) << 32 | raw_data[212])
      .SetData107(static_cast<int64_t>(raw_data[215]) << 32 | raw_data[214])
      .SetData108(static_cast<int64_t>(raw_data[217]) << 32 | raw_data[216])
      .SetData109(static_cast<int64_t>(raw_data[219]) << 32 | raw_data[218])
      .SetData110(static_cast<int64_t>(raw_data[221]) << 32 | raw_data[220])
      .SetData111(static_cast<int64_t>(raw_data[223]) << 32 | raw_data[222])
      .SetData112(static_cast<int64_t>(raw_data[225]) << 32 | raw_data[224])
      .SetData113(static_cast<int64_t>(raw_data[227]) << 32 | raw_data[226])
      .SetData114(static_cast<int64_t>(raw_data[229]) << 32 | raw_data[228])
      .SetData115(static_cast<int64_t>(raw_data[231]) << 32 | raw_data[230])
      .SetData116(static_cast<int64_t>(raw_data[233]) << 32 | raw_data[232])
      .SetData117(static_cast<int64_t>(raw_data[235]) << 32 | raw_data[234])
      .SetData118(static_cast<int64_t>(raw_data[237]) << 32 | raw_data[236])
      .SetData119(static_cast<int64_t>(raw_data[239]) << 32 | raw_data[238])
      .SetData120(static_cast<int64_t>(raw_data[241]) << 32 | raw_data[240])
      .SetData121(static_cast<int64_t>(raw_data[243]) << 32 | raw_data[242])
      .SetData122(static_cast<int64_t>(raw_data[245]) << 32 | raw_data[244])
      .SetData123(static_cast<int64_t>(raw_data[247]) << 32 | raw_data[246])
      .SetData124(static_cast<int64_t>(raw_data[249]) << 32 | raw_data[248])
      .SetData125(static_cast<int64_t>(raw_data[251]) << 32 | raw_data[250])
      .SetData126(static_cast<int64_t>(raw_data[253]) << 32 | raw_data[252])
      .SetData127(static_cast<int64_t>(raw_data[255]) << 32 | raw_data[254])
      .SetData128(static_cast<int64_t>(raw_data[257]) << 32 | raw_data[256])
      .SetData129(static_cast<int64_t>(raw_data[259]) << 32 | raw_data[258])
      .SetData130(static_cast<int64_t>(raw_data[261]) << 32 | raw_data[260])
      .SetData131(static_cast<int64_t>(raw_data[263]) << 32 | raw_data[262])
      .SetData132(static_cast<int64_t>(raw_data[265]) << 32 | raw_data[264])
      .SetData133(static_cast<int64_t>(raw_data[267]) << 32 | raw_data[266])
      .SetData134(static_cast<int64_t>(raw_data[269]) << 32 | raw_data[268])
      .SetData135(static_cast<int64_t>(raw_data[271]) << 32 | raw_data[270])
      .SetData136(static_cast<int64_t>(raw_data[273]) << 32 | raw_data[272])
      .SetData137(static_cast<int64_t>(raw_data[275]) << 32 | raw_data[274])
      .SetData138(static_cast<int64_t>(raw_data[277]) << 32 | raw_data[276])
      .SetData139(static_cast<int64_t>(raw_data[279]) << 32 | raw_data[278])
      .SetData140(static_cast<int64_t>(raw_data[281]) << 32 | raw_data[280])
      .SetData141(static_cast<int64_t>(raw_data[283]) << 32 | raw_data[282])
      .SetData142(static_cast<int64_t>(raw_data[285]) << 32 | raw_data[284])
      .SetData143(static_cast<int64_t>(raw_data[287]) << 32 | raw_data[286])
      .SetData144(static_cast<int64_t>(raw_data[289]) << 32 | raw_data[288])
      .SetData145(static_cast<int64_t>(raw_data[291]) << 32 | raw_data[290])
      .SetData146(static_cast<int64_t>(raw_data[293]) << 32 | raw_data[292])
      .SetData147(static_cast<int64_t>(raw_data[295]) << 32 | raw_data[294])
      .SetData148(static_cast<int64_t>(raw_data[297]) << 32 | raw_data[296])
      .SetData149(static_cast<int64_t>(raw_data[299]) << 32 | raw_data[298])
      .SetData150(static_cast<int64_t>(raw_data[301]) << 32 | raw_data[300])
      .SetData151(static_cast<int64_t>(raw_data[303]) << 32 | raw_data[302])
      .SetData152(static_cast<int64_t>(raw_data[305]) << 32 | raw_data[304])
      .SetData153(static_cast<int64_t>(raw_data[307]) << 32 | raw_data[306])
      .SetData154(static_cast<int64_t>(raw_data[309]) << 32 | raw_data[308])
      .SetData155(static_cast<int64_t>(raw_data[311]) << 32 | raw_data[310])
      .SetData156(static_cast<int64_t>(raw_data[313]) << 32 | raw_data[312])
      .SetData157(static_cast<int64_t>(raw_data[315]) << 32 | raw_data[314])
      .SetData158(static_cast<int64_t>(raw_data[317]) << 32 | raw_data[316])
      .SetData159(static_cast<int64_t>(raw_data[319]) << 32 | raw_data[318])
      .SetData160(static_cast<int64_t>(raw_data[321]) << 32 | raw_data[320])
      .SetData161(static_cast<int64_t>(raw_data[323]) << 32 | raw_data[322])
      .SetData162(static_cast<int64_t>(raw_data[325]) << 32 | raw_data[324])
      .SetData163(static_cast<int64_t>(raw_data[327]) << 32 | raw_data[326])
      .SetData164(static_cast<int64_t>(raw_data[329]) << 32 | raw_data[328])
      .SetData165(static_cast<int64_t>(raw_data[331]) << 32 | raw_data[330])
      .SetData166(static_cast<int64_t>(raw_data[333]) << 32 | raw_data[332])
      .SetData167(static_cast<int64_t>(raw_data[335]) << 32 | raw_data[334])
      .SetData168(static_cast<int64_t>(raw_data[337]) << 32 | raw_data[336])
      .SetData169(static_cast<int64_t>(raw_data[339]) << 32 | raw_data[338])
      .SetData170(static_cast<int64_t>(raw_data[341]) << 32 | raw_data[340])
      .SetData171(static_cast<int64_t>(raw_data[343]) << 32 | raw_data[342])
      .SetData172(static_cast<int64_t>(raw_data[345]) << 32 | raw_data[344])
      .SetData173(static_cast<int64_t>(raw_data[347]) << 32 | raw_data[346])
      .SetData174(static_cast<int64_t>(raw_data[349]) << 32 | raw_data[348])
      .SetData175(static_cast<int64_t>(raw_data[351]) << 32 | raw_data[350])
      .SetData176(static_cast<int64_t>(raw_data[353]) << 32 | raw_data[352])
      .SetData177(static_cast<int64_t>(raw_data[355]) << 32 | raw_data[354])
      .SetData178(static_cast<int64_t>(raw_data[357]) << 32 | raw_data[356])
      .SetData179(static_cast<int64_t>(raw_data[359]) << 32 | raw_data[358])
      .SetData180(static_cast<int64_t>(raw_data[361]) << 32 | raw_data[360])
      .SetData181(static_cast<int64_t>(raw_data[363]) << 32 | raw_data[362])
      .SetData182(static_cast<int64_t>(raw_data[365]) << 32 | raw_data[364])
      .SetData183(static_cast<int64_t>(raw_data[367]) << 32 | raw_data[366])
      .SetData184(static_cast<int64_t>(raw_data[369]) << 32 | raw_data[368])
      .SetData185(static_cast<int64_t>(raw_data[371]) << 32 | raw_data[370])
      .SetData186(static_cast<int64_t>(raw_data[373]) << 32 | raw_data[372])
      .SetData187(static_cast<int64_t>(raw_data[375]) << 32 | raw_data[374])
      .SetData188(static_cast<int64_t>(raw_data[377]) << 32 | raw_data[376])
      .SetData189(static_cast<int64_t>(raw_data[379]) << 32 | raw_data[378])
      .SetData190(static_cast<int64_t>(raw_data[381]) << 32 | raw_data[380])
      .SetData191(static_cast<int64_t>(raw_data[383]) << 32 | raw_data[382])
      .SetData192(static_cast<int64_t>(raw_data[385]) << 32 | raw_data[384])
      .SetData193(static_cast<int64_t>(raw_data[387]) << 32 | raw_data[386])
      .SetData194(static_cast<int64_t>(raw_data[389]) << 32 | raw_data[388])
      .SetData195(static_cast<int64_t>(raw_data[391]) << 32 | raw_data[390])
      .SetData196(static_cast<int64_t>(raw_data[393]) << 32 | raw_data[392])
      .SetData197(static_cast<int64_t>(raw_data[395]) << 32 | raw_data[394])
      .SetData198(static_cast<int64_t>(raw_data[397]) << 32 | raw_data[396])
      .SetData199(static_cast<int64_t>(raw_data[399]) << 32 | raw_data[398])
      .SetData200(static_cast<int64_t>(raw_data[401]) << 32 | raw_data[400])
      .SetData201(static_cast<int64_t>(raw_data[403]) << 32 | raw_data[402])
      .SetData202(static_cast<int64_t>(raw_data[405]) << 32 | raw_data[404])
      .SetData203(static_cast<int64_t>(raw_data[407]) << 32 | raw_data[406])
      .SetData204(static_cast<int64_t>(raw_data[409]) << 32 | raw_data[408])
      .SetData205(static_cast<int64_t>(raw_data[411]) << 32 | raw_data[410])
      .SetData206(static_cast<int64_t>(raw_data[413]) << 32 | raw_data[412])
      .SetData207(static_cast<int64_t>(raw_data[415]) << 32 | raw_data[414])
      .SetData208(static_cast<int64_t>(raw_data[417]) << 32 | raw_data[416])
      .SetData209(static_cast<int64_t>(raw_data[419]) << 32 | raw_data[418])
      .SetData210(static_cast<int64_t>(raw_data[421]) << 32 | raw_data[420])
      .SetData211(static_cast<int64_t>(raw_data[423]) << 32 | raw_data[422])
      .SetData212(static_cast<int64_t>(raw_data[425]) << 32 | raw_data[424])
      .SetData213(static_cast<int64_t>(raw_data[427]) << 32 | raw_data[426])
      .SetData214(static_cast<int64_t>(raw_data[429]) << 32 | raw_data[428])
      .SetData215(static_cast<int64_t>(raw_data[431]) << 32 | raw_data[430])
      .SetData216(static_cast<int64_t>(raw_data[433]) << 32 | raw_data[432])
      .SetData217(static_cast<int64_t>(raw_data[435]) << 32 | raw_data[434])
      .SetData218(static_cast<int64_t>(raw_data[437]) << 32 | raw_data[436])
      .SetData219(static_cast<int64_t>(raw_data[439]) << 32 | raw_data[438])
      .SetData220(static_cast<int64_t>(raw_data[441]) << 32 | raw_data[440])
      .SetData221(static_cast<int64_t>(raw_data[443]) << 32 | raw_data[442])
      .SetData222(static_cast<int64_t>(raw_data[445]) << 32 | raw_data[444])
      .SetData223(static_cast<int64_t>(raw_data[447]) << 32 | raw_data[446])
      .SetData224(static_cast<int64_t>(raw_data[449]) << 32 | raw_data[448])
      .SetData225(static_cast<int64_t>(raw_data[451]) << 32 | raw_data[450])
      .SetData226(static_cast<int64_t>(raw_data[453]) << 32 | raw_data[452])
      .SetData227(static_cast<int64_t>(raw_data[455]) << 32 | raw_data[454])
      .SetData228(static_cast<int64_t>(raw_data[457]) << 32 | raw_data[456])
      .SetData229(static_cast<int64_t>(raw_data[459]) << 32 | raw_data[458])
      .SetData230(static_cast<int64_t>(raw_data[461]) << 32 | raw_data[460])
      .SetData231(static_cast<int64_t>(raw_data[463]) << 32 | raw_data[462])
      .SetData232(static_cast<int64_t>(raw_data[465]) << 32 | raw_data[464])
      .SetData233(static_cast<int64_t>(raw_data[467]) << 32 | raw_data[466])
      .SetData234(static_cast<int64_t>(raw_data[469]) << 32 | raw_data[468])
      .SetData235(static_cast<int64_t>(raw_data[471]) << 32 | raw_data[470])
      .SetData236(static_cast<int64_t>(raw_data[473]) << 32 | raw_data[472])
      .SetData237(static_cast<int64_t>(raw_data[475]) << 32 | raw_data[474])
      .SetData238(static_cast<int64_t>(raw_data[477]) << 32 | raw_data[476])
      .SetData239(static_cast<int64_t>(raw_data[479]) << 32 | raw_data[478])
      .SetData240(static_cast<int64_t>(raw_data[481]) << 32 | raw_data[480])
      .SetData241(static_cast<int64_t>(raw_data[483]) << 32 | raw_data[482])
      .SetData242(static_cast<int64_t>(raw_data[485]) << 32 | raw_data[484])
      .SetData243(static_cast<int64_t>(raw_data[487]) << 32 | raw_data[486])
      .SetData244(static_cast<int64_t>(raw_data[489]) << 32 | raw_data[488])
      .SetData245(static_cast<int64_t>(raw_data[491]) << 32 | raw_data[490])
      .SetData246(static_cast<int64_t>(raw_data[493]) << 32 | raw_data[492])
      .SetData247(static_cast<int64_t>(raw_data[495]) << 32 | raw_data[494])
      .SetData248(static_cast<int64_t>(raw_data[497]) << 32 | raw_data[496])
      .SetData249(static_cast<int64_t>(raw_data[499]) << 32 | raw_data[498])
      .SetData250(static_cast<int64_t>(raw_data[501]) << 32 | raw_data[500])
      .SetData251(static_cast<int64_t>(raw_data[503]) << 32 | raw_data[502])
      .SetData252(static_cast<int64_t>(raw_data[505]) << 32 | raw_data[504])
      .SetData253(static_cast<int64_t>(raw_data[507]) << 32 | raw_data[506])
      .SetData254(static_cast<int64_t>(raw_data[509]) << 32 | raw_data[508])
      .SetData255(static_cast<int64_t>(raw_data[511]) << 32 | raw_data[510])
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
