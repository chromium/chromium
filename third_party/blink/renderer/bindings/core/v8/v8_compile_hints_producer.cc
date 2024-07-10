// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints_producer.h"

#if BUILDFLAG(PRODUCE_V8_COMPILE_HINTS)

#include "base/rand_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints_common.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/bloom_filter.h"

#include <limits>

namespace blink::v8_compile_hints {

std::atomic<bool>
    V8CrowdsourcedCompileHintsProducer::data_generated_for_this_process_ =
        false;

namespace {

bool RandomlySelectedToGenerateData() {
  // Data collection is only enabled on Windows. TODO(chromium:1406506): enable
  // on more platforms.
#if BUILDFLAG(IS_WIN)
  bool compile_hints_enabled =
      base::FeatureList::IsEnabled(features::kProduceCompileHints2);
  if (!compile_hints_enabled) {
    return false;
  }

  // Decide whether we collect the data based on client-side randomization.
  // This is further subject to UKM restrictions: whether the user has enabled
  // the data collection + downsampling. See crbug.com/1483975 .
  double data_production_level =
      features::kProduceCompileHintsDataProductionLevel.Get();
  return base::RandDouble() < data_production_level;
#else   //  BUILDFLAG(IS_WIN)
  return false;
#endif  //  BUILDFLAG(IS_WIN)
}

bool ShouldThisProcessGenerateData() {
  if (base::FeatureList::IsEnabled(features::kForceProduceCompileHints)) {
    return true;
  }
  static bool randomly_selected_to_generate_data =
      RandomlySelectedToGenerateData();
  return randomly_selected_to_generate_data;
}

}  // namespace

V8CrowdsourcedCompileHintsProducer::V8CrowdsourcedCompileHintsProducer(
    Page* page)
    : page_(page) {
  // Decide whether to produce the data once per renderer process.
  bool should_generate_data = ShouldThisProcessGenerateData();
  if (should_generate_data && !data_generated_for_this_process_) {
    state_ = State::kCollectingData;
  }
}

void V8CrowdsourcedCompileHintsProducer::RecordScript(
    Frame* frame,
    ExecutionContext* execution_context,
    const v8::Local<v8::Script> script,
    ScriptState* script_state) {
  if (state_ != State::kCollectingData) {
    // We've already generated data for this V8CrowdsourcedCompileHintsProducer,
    // or data generation is disabled. Don't record any script compilations.
    return;
  }
  if (data_generated_for_this_process_) {
    // We've already generated data for some other
    // V8CrowdsourcedCompileHintsProducer, so stop collecting data.
    ClearData();
    return;
  }

  v8::Isolate* isolate = execution_context->GetIsolate();
  v8::Local<v8::Context> context = script_state->GetContext();
  uint32_t script_name_hash =
      ScriptNameHash(script->GetResourceName(), context, isolate);

  compile_hints_collectors_.emplace_back(isolate,
                                         script->GetCompileHintsCollector());
  script_name_hashes_.emplace_back(script_name_hash);

  if (compile_hints_collectors_.size() == 1) {
    ScheduleDataDeletionTask(execution_context);
  }
}

void V8CrowdsourcedCompileHintsProducer::GenerateData() {
  // Guard against this function getting called repeatedly.
  if (state_ != State::kCollectingData) {
    return;
  }

  if (!data_generated_for_this_process_) {
    data_generated_for_this_process_ = SendDataToUkm();
  }

  ClearData();
}

void V8CrowdsourcedCompileHintsProducer::Trace(Visitor* visitor) const {
  visitor->Trace(page_);
  visitor->Trace(compile_hints_collectors_);
}

void V8CrowdsourcedCompileHintsProducer::ClearData() {
  // Stop logging script executions for this page.
  state_ = State::kFinishedOrDisabled;
  compile_hints_collectors_.clear();
  script_name_hashes_.clear();
}

namespace {

void ClearDataTask(V8CrowdsourcedCompileHintsProducer* producer) {
  if (producer != nullptr) {
    producer->ClearData();
  }
}

}  // namespace

void V8CrowdsourcedCompileHintsProducer::ScheduleDataDeletionTask(
    ExecutionContext* execution_context) {
  constexpr int kDeletionDelaySeconds = 30;
  auto delay = base::Seconds(kDeletionDelaySeconds);

  execution_context->GetTaskRunner(TaskType::kIdleTask)
      ->PostDelayedTask(FROM_HERE,
                        WTF::BindOnce(&ClearDataTask, WrapWeakPersistent(this)),
                        delay);
}

bool V8CrowdsourcedCompileHintsProducer::MightGenerateData() {
  if (state_ != State::kCollectingData || data_generated_for_this_process_) {
    return false;
  }

  Frame* main_frame = page_->MainFrame();
  // Because of OOPIF, the main frame is not necessarily a LocalFrame. We cannot
  // generate good compile hints, because we cannot retrieve data from other
  // processes.
  if (!main_frame->IsLocalFrame()) {
    ClearData();
    return false;
  }
  return true;
}

bool V8CrowdsourcedCompileHintsProducer::SendDataToUkm() {
  // Re-check the main frame, since it might have changed.
  Frame* main_frame = page_->MainFrame();
  if (!main_frame->IsLocalFrame()) {
    ClearData();
    return false;
  }

  ScriptState* script_state =
      ToScriptStateForMainWorld(DynamicTo<LocalFrame>(main_frame));
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  v8::Isolate* isolate = execution_context->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  int total_funcs = 0;

  DCHECK_EQ(compile_hints_collectors_.size(), script_name_hashes_.size());

  // Create a Bloom filter w/ 16 key bits. This results in a Bloom filter
  // containing 2 ^ 16 bits, which equals to 1024 64-bit ints.
  static_assert((1 << kBloomFilterKeySize) / (sizeof(int32_t) * 8) ==
                kBloomFilterInt32Count);
  WTF::BloomFilter<kBloomFilterKeySize> bloom;

  for (wtf_size_t script_ix = 0; script_ix < compile_hints_collectors_.size();
       ++script_ix) {
    v8::Local<v8::CompileHintsCollector> compile_hints_collector =
        compile_hints_collectors_[script_ix].Get(isolate);
    std::vector<int> compile_hints =
        compile_hints_collector->GetCompileHints(isolate);
    for (int function_position : compile_hints) {
      uint32_t hash =
          CombineHash(script_name_hashes_[script_ix], function_position);
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
  ukm::builders::V8CompileHints_Version5(execution_context->UkmSourceID())
      .SetData0(static_cast<int64_t>(raw_data[1]) << 32 | raw_data[0])
      .SetData1(static_cast<int64_t>(raw_data[3]) << 32 | raw_data[2])
      .SetData2(static_cast<int64_t>(raw_data[5]) << 32 | raw_data[4])
      .SetData3(static_cast<int64_t>(raw_data[7]) << 32 | raw_data[6])
      .SetData4(static_cast<int64_t>(raw_data[9]) << 32 | raw_data[8])
      .SetData5(static_cast<int64_t>(raw_data[11]) << 32 | raw_data[10])
      .SetData6(static_cast<int64_t>(raw_data[13]) << 32 | raw_data[12])
      .SetData7(static_cast<int64_t>(raw_data[15]) << 32 | raw_data[14])
      .SetData8(static_cast<int64_t>(raw_data[17]) << 32 | raw_data[16])
      .SetData9(static_cast<int64_t>(raw_data[19]) << 32 | raw_data[18])
      .SetData10(static_cast<int64_t>(raw_data[21]) << 32 | raw_data[20])
      .SetData11(static_cast<int64_t>(raw_data[23]) << 32 | raw_data[22])
      .SetData12(static_cast<int64_t>(raw_data[25]) << 32 | raw_data[24])
      .SetData13(static_cast<int64_t>(raw_data[27]) << 32 | raw_data[26])
      .SetData14(static_cast<int64_t>(raw_data[29]) << 32 | raw_data[28])
      .SetData15(static_cast<int64_t>(raw_data[31]) << 32 | raw_data[30])
      .SetData16(static_cast<int64_t>(raw_data[33]) << 32 | raw_data[32])
      .SetData17(static_cast<int64_t>(raw_data[35]) << 32 | raw_data[34])
      .SetData18(static_cast<int64_t>(raw_data[37]) << 32 | raw_data[36])
      .SetData19(static_cast<int64_t>(raw_data[39]) << 32 | raw_data[38])
      .SetData20(static_cast<int64_t>(raw_data[41]) << 32 | raw_data[40])
      .SetData21(static_cast<int64_t>(raw_data[43]) << 32 | raw_data[42])
      .SetData22(static_cast<int64_t>(raw_data[45]) << 32 | raw_data[44])
      .SetData23(static_cast<int64_t>(raw_data[47]) << 32 | raw_data[46])
      .SetData24(static_cast<int64_t>(raw_data[49]) << 32 | raw_data[48])
      .SetData25(static_cast<int64_t>(raw_data[51]) << 32 | raw_data[50])
      .SetData26(static_cast<int64_t>(raw_data[53]) << 32 | raw_data[52])
      .SetData27(static_cast<int64_t>(raw_data[55]) << 32 | raw_data[54])
      .SetData28(static_cast<int64_t>(raw_data[57]) << 32 | raw_data[56])
      .SetData29(static_cast<int64_t>(raw_data[59]) << 32 | raw_data[58])
      .SetData30(static_cast<int64_t>(raw_data[61]) << 32 | raw_data[60])
      .SetData31(static_cast<int64_t>(raw_data[63]) << 32 | raw_data[62])
      .SetData32(static_cast<int64_t>(raw_data[65]) << 32 | raw_data[64])
      .SetData33(static_cast<int64_t>(raw_data[67]) << 32 | raw_data[66])
      .SetData34(static_cast<int64_t>(raw_data[69]) << 32 | raw_data[68])
      .SetData35(static_cast<int64_t>(raw_data[71]) << 32 | raw_data[70])
      .SetData36(static_cast<int64_t>(raw_data[73]) << 32 | raw_data[72])
      .SetData37(static_cast<int64_t>(raw_data[75]) << 32 | raw_data[74])
      .SetData38(static_cast<int64_t>(raw_data[77]) << 32 | raw_data[76])
      .SetData39(static_cast<int64_t>(raw_data[79]) << 32 | raw_data[78])
      .SetData40(static_cast<int64_t>(raw_data[81]) << 32 | raw_data[80])
      .SetData41(static_cast<int64_t>(raw_data[83]) << 32 | raw_data[82])
      .SetData42(static_cast<int64_t>(raw_data[85]) << 32 | raw_data[84])
      .SetData43(static_cast<int64_t>(raw_data[87]) << 32 | raw_data[86])
      .SetData44(static_cast<int64_t>(raw_data[89]) << 32 | raw_data[88])
      .SetData45(static_cast<int64_t>(raw_data[91]) << 32 | raw_data[90])
      .SetData46(static_cast<int64_t>(raw_data[93]) << 32 | raw_data[92])
      .SetData47(static_cast<int64_t>(raw_data[95]) << 32 | raw_data[94])
      .SetData48(static_cast<int64_t>(raw_data[97]) << 32 | raw_data[96])
      .SetData49(static_cast<int64_t>(raw_data[99]) << 32 | raw_data[98])
      .SetData50(static_cast<int64_t>(raw_data[101]) << 32 | raw_data[100])
      .SetData51(static_cast<int64_t>(raw_data[103]) << 32 | raw_data[102])
      .SetData52(static_cast<int64_t>(raw_data[105]) << 32 | raw_data[104])
      .SetData53(static_cast<int64_t>(raw_data[107]) << 32 | raw_data[106])
      .SetData54(static_cast<int64_t>(raw_data[109]) << 32 | raw_data[108])
      .SetData55(static_cast<int64_t>(raw_data[111]) << 32 | raw_data[110])
      .SetData56(static_cast<int64_t>(raw_data[113]) << 32 | raw_data[112])
      .SetData57(static_cast<int64_t>(raw_data[115]) << 32 | raw_data[114])
      .SetData58(static_cast<int64_t>(raw_data[117]) << 32 | raw_data[116])
      .SetData59(static_cast<int64_t>(raw_data[119]) << 32 | raw_data[118])
      .SetData60(static_cast<int64_t>(raw_data[121]) << 32 | raw_data[120])
      .SetData61(static_cast<int64_t>(raw_data[123]) << 32 | raw_data[122])
      .SetData62(static_cast<int64_t>(raw_data[125]) << 32 | raw_data[124])
      .SetData63(static_cast<int64_t>(raw_data[127]) << 32 | raw_data[126])
      .SetData64(static_cast<int64_t>(raw_data[129]) << 32 | raw_data[128])
      .SetData65(static_cast<int64_t>(raw_data[131]) << 32 | raw_data[130])
      .SetData66(static_cast<int64_t>(raw_data[133]) << 32 | raw_data[132])
      .SetData67(static_cast<int64_t>(raw_data[135]) << 32 | raw_data[134])
      .SetData68(static_cast<int64_t>(raw_data[137]) << 32 | raw_data[136])
      .SetData69(static_cast<int64_t>(raw_data[139]) << 32 | raw_data[138])
      .SetData70(static_cast<int64_t>(raw_data[141]) << 32 | raw_data[140])
      .SetData71(static_cast<int64_t>(raw_data[143]) << 32 | raw_data[142])
      .SetData72(static_cast<int64_t>(raw_data[145]) << 32 | raw_data[144])
      .SetData73(static_cast<int64_t>(raw_data[147]) << 32 | raw_data[146])
      .SetData74(static_cast<int64_t>(raw_data[149]) << 32 | raw_data[148])
      .SetData75(static_cast<int64_t>(raw_data[151]) << 32 | raw_data[150])
      .SetData76(static_cast<int64_t>(raw_data[153]) << 32 | raw_data[152])
      .SetData77(static_cast<int64_t>(raw_data[155]) << 32 | raw_data[154])
      .SetData78(static_cast<int64_t>(raw_data[157]) << 32 | raw_data[156])
      .SetData79(static_cast<int64_t>(raw_data[159]) << 32 | raw_data[158])
      .SetData80(static_cast<int64_t>(raw_data[161]) << 32 | raw_data[160])
      .SetData81(static_cast<int64_t>(raw_data[163]) << 32 | raw_data[162])
      .SetData82(static_cast<int64_t>(raw_data[165]) << 32 | raw_data[164])
      .SetData83(static_cast<int64_t>(raw_data[167]) << 32 | raw_data[166])
      .SetData84(static_cast<int64_t>(raw_data[169]) << 32 | raw_data[168])
      .SetData85(static_cast<int64_t>(raw_data[171]) << 32 | raw_data[170])
      .SetData86(static_cast<int64_t>(raw_data[173]) << 32 | raw_data[172])
      .SetData87(static_cast<int64_t>(raw_data[175]) << 32 | raw_data[174])
      .SetData88(static_cast<int64_t>(raw_data[177]) << 32 | raw_data[176])
      .SetData89(static_cast<int64_t>(raw_data[179]) << 32 | raw_data[178])
      .SetData90(static_cast<int64_t>(raw_data[181]) << 32 | raw_data[180])
      .SetData91(static_cast<int64_t>(raw_data[183]) << 32 | raw_data[182])
      .SetData92(static_cast<int64_t>(raw_data[185]) << 32 | raw_data[184])
      .SetData93(static_cast<int64_t>(raw_data[187]) << 32 | raw_data[186])
      .SetData94(static_cast<int64_t>(raw_data[189]) << 32 | raw_data[188])
      .SetData95(static_cast<int64_t>(raw_data[191]) << 32 | raw_data[190])
      .SetData96(static_cast<int64_t>(raw_data[193]) << 32 | raw_data[192])
      .SetData97(static_cast<int64_t>(raw_data[195]) << 32 | raw_data[194])
      .SetData98(static_cast<int64_t>(raw_data[197]) << 32 | raw_data[196])
      .SetData99(static_cast<int64_t>(raw_data[199]) << 32 | raw_data[198])
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
      .SetData256(static_cast<int64_t>(raw_data[513]) << 32 | raw_data[512])
      .SetData257(static_cast<int64_t>(raw_data[515]) << 32 | raw_data[514])
      .SetData258(static_cast<int64_t>(raw_data[517]) << 32 | raw_data[516])
      .SetData259(static_cast<int64_t>(raw_data[519]) << 32 | raw_data[518])
      .SetData260(static_cast<int64_t>(raw_data[521]) << 32 | raw_data[520])
      .SetData261(static_cast<int64_t>(raw_data[523]) << 32 | raw_data[522])
      .SetData262(static_cast<int64_t>(raw_data[525]) << 32 | raw_data[524])
      .SetData263(static_cast<int64_t>(raw_data[527]) << 32 | raw_data[526])
      .SetData264(static_cast<int64_t>(raw_data[529]) << 32 | raw_data[528])
      .SetData265(static_cast<int64_t>(raw_data[531]) << 32 | raw_data[530])
      .SetData266(static_cast<int64_t>(raw_data[533]) << 32 | raw_data[532])
      .SetData267(static_cast<int64_t>(raw_data[535]) << 32 | raw_data[534])
      .SetData268(static_cast<int64_t>(raw_data[537]) << 32 | raw_data[536])
      .SetData269(static_cast<int64_t>(raw_data[539]) << 32 | raw_data[538])
      .SetData270(static_cast<int64_t>(raw_data[541]) << 32 | raw_data[540])
      .SetData271(static_cast<int64_t>(raw_data[543]) << 32 | raw_data[542])
      .SetData272(static_cast<int64_t>(raw_data[545]) << 32 | raw_data[544])
      .SetData273(static_cast<int64_t>(raw_data[547]) << 32 | raw_data[546])
      .SetData274(static_cast<int64_t>(raw_data[549]) << 32 | raw_data[548])
      .SetData275(static_cast<int64_t>(raw_data[551]) << 32 | raw_data[550])
      .SetData276(static_cast<int64_t>(raw_data[553]) << 32 | raw_data[552])
      .SetData277(static_cast<int64_t>(raw_data[555]) << 32 | raw_data[554])
      .SetData278(static_cast<int64_t>(raw_data[557]) << 32 | raw_data[556])
      .SetData279(static_cast<int64_t>(raw_data[559]) << 32 | raw_data[558])
      .SetData280(static_cast<int64_t>(raw_data[561]) << 32 | raw_data[560])
      .SetData281(static_cast<int64_t>(raw_data[563]) << 32 | raw_data[562])
      .SetData282(static_cast<int64_t>(raw_data[565]) << 32 | raw_data[564])
      .SetData283(static_cast<int64_t>(raw_data[567]) << 32 | raw_data[566])
      .SetData284(static_cast<int64_t>(raw_data[569]) << 32 | raw_data[568])
      .SetData285(static_cast<int64_t>(raw_data[571]) << 32 | raw_data[570])
      .SetData286(static_cast<int64_t>(raw_data[573]) << 32 | raw_data[572])
      .SetData287(static_cast<int64_t>(raw_data[575]) << 32 | raw_data[574])
      .SetData288(static_cast<int64_t>(raw_data[577]) << 32 | raw_data[576])
      .SetData289(static_cast<int64_t>(raw_data[579]) << 32 | raw_data[578])
      .SetData290(static_cast<int64_t>(raw_data[581]) << 32 | raw_data[580])
      .SetData291(static_cast<int64_t>(raw_data[583]) << 32 | raw_data[582])
      .SetData292(static_cast<int64_t>(raw_data[585]) << 32 | raw_data[584])
      .SetData293(static_cast<int64_t>(raw_data[587]) << 32 | raw_data[586])
      .SetData294(static_cast<int64_t>(raw_data[589]) << 32 | raw_data[588])
      .SetData295(static_cast<int64_t>(raw_data[591]) << 32 | raw_data[590])
      .SetData296(static_cast<int64_t>(raw_data[593]) << 32 | raw_data[592])
      .SetData297(static_cast<int64_t>(raw_data[595]) << 32 | raw_data[594])
      .SetData298(static_cast<int64_t>(raw_data[597]) << 32 | raw_data[596])
      .SetData299(static_cast<int64_t>(raw_data[599]) << 32 | raw_data[598])
      .SetData300(static_cast<int64_t>(raw_data[601]) << 32 | raw_data[600])
      .SetData301(static_cast<int64_t>(raw_data[603]) << 32 | raw_data[602])
      .SetData302(static_cast<int64_t>(raw_data[605]) << 32 | raw_data[604])
      .SetData303(static_cast<int64_t>(raw_data[607]) << 32 | raw_data[606])
      .SetData304(static_cast<int64_t>(raw_data[609]) << 32 | raw_data[608])
      .SetData305(static_cast<int64_t>(raw_data[611]) << 32 | raw_data[610])
      .SetData306(static_cast<int64_t>(raw_data[613]) << 32 | raw_data[612])
      .SetData307(static_cast<int64_t>(raw_data[615]) << 32 | raw_data[614])
      .SetData308(static_cast<int64_t>(raw_data[617]) << 32 | raw_data[616])
      .SetData309(static_cast<int64_t>(raw_data[619]) << 32 | raw_data[618])
      .SetData310(static_cast<int64_t>(raw_data[621]) << 32 | raw_data[620])
      .SetData311(static_cast<int64_t>(raw_data[623]) << 32 | raw_data[622])
      .SetData312(static_cast<int64_t>(raw_data[625]) << 32 | raw_data[624])
      .SetData313(static_cast<int64_t>(raw_data[627]) << 32 | raw_data[626])
      .SetData314(static_cast<int64_t>(raw_data[629]) << 32 | raw_data[628])
      .SetData315(static_cast<int64_t>(raw_data[631]) << 32 | raw_data[630])
      .SetData316(static_cast<int64_t>(raw_data[633]) << 32 | raw_data[632])
      .SetData317(static_cast<int64_t>(raw_data[635]) << 32 | raw_data[634])
      .SetData318(static_cast<int64_t>(raw_data[637]) << 32 | raw_data[636])
      .SetData319(static_cast<int64_t>(raw_data[639]) << 32 | raw_data[638])
      .SetData320(static_cast<int64_t>(raw_data[641]) << 32 | raw_data[640])
      .SetData321(static_cast<int64_t>(raw_data[643]) << 32 | raw_data[642])
      .SetData322(static_cast<int64_t>(raw_data[645]) << 32 | raw_data[644])
      .SetData323(static_cast<int64_t>(raw_data[647]) << 32 | raw_data[646])
      .SetData324(static_cast<int64_t>(raw_data[649]) << 32 | raw_data[648])
      .SetData325(static_cast<int64_t>(raw_data[651]) << 32 | raw_data[650])
      .SetData326(static_cast<int64_t>(raw_data[653]) << 32 | raw_data[652])
      .SetData327(static_cast<int64_t>(raw_data[655]) << 32 | raw_data[654])
      .SetData328(static_cast<int64_t>(raw_data[657]) << 32 | raw_data[656])
      .SetData329(static_cast<int64_t>(raw_data[659]) << 32 | raw_data[658])
      .SetData330(static_cast<int64_t>(raw_data[661]) << 32 | raw_data[660])
      .SetData331(static_cast<int64_t>(raw_data[663]) << 32 | raw_data[662])
      .SetData332(static_cast<int64_t>(raw_data[665]) << 32 | raw_data[664])
      .SetData333(static_cast<int64_t>(raw_data[667]) << 32 | raw_data[666])
      .SetData334(static_cast<int64_t>(raw_data[669]) << 32 | raw_data[668])
      .SetData335(static_cast<int64_t>(raw_data[671]) << 32 | raw_data[670])
      .SetData336(static_cast<int64_t>(raw_data[673]) << 32 | raw_data[672])
      .SetData337(static_cast<int64_t>(raw_data[675]) << 32 | raw_data[674])
      .SetData338(static_cast<int64_t>(raw_data[677]) << 32 | raw_data[676])
      .SetData339(static_cast<int64_t>(raw_data[679]) << 32 | raw_data[678])
      .SetData340(static_cast<int64_t>(raw_data[681]) << 32 | raw_data[680])
      .SetData341(static_cast<int64_t>(raw_data[683]) << 32 | raw_data[682])
      .SetData342(static_cast<int64_t>(raw_data[685]) << 32 | raw_data[684])
      .SetData343(static_cast<int64_t>(raw_data[687]) << 32 | raw_data[686])
      .SetData344(static_cast<int64_t>(raw_data[689]) << 32 | raw_data[688])
      .SetData345(static_cast<int64_t>(raw_data[691]) << 32 | raw_data[690])
      .SetData346(static_cast<int64_t>(raw_data[693]) << 32 | raw_data[692])
      .SetData347(static_cast<int64_t>(raw_data[695]) << 32 | raw_data[694])
      .SetData348(static_cast<int64_t>(raw_data[697]) << 32 | raw_data[696])
      .SetData349(static_cast<int64_t>(raw_data[699]) << 32 | raw_data[698])
      .SetData350(static_cast<int64_t>(raw_data[701]) << 32 | raw_data[700])
      .SetData351(static_cast<int64_t>(raw_data[703]) << 32 | raw_data[702])
      .SetData352(static_cast<int64_t>(raw_data[705]) << 32 | raw_data[704])
      .SetData353(static_cast<int64_t>(raw_data[707]) << 32 | raw_data[706])
      .SetData354(static_cast<int64_t>(raw_data[709]) << 32 | raw_data[708])
      .SetData355(static_cast<int64_t>(raw_data[711]) << 32 | raw_data[710])
      .SetData356(static_cast<int64_t>(raw_data[713]) << 32 | raw_data[712])
      .SetData357(static_cast<int64_t>(raw_data[715]) << 32 | raw_data[714])
      .SetData358(static_cast<int64_t>(raw_data[717]) << 32 | raw_data[716])
      .SetData359(static_cast<int64_t>(raw_data[719]) << 32 | raw_data[718])
      .SetData360(static_cast<int64_t>(raw_data[721]) << 32 | raw_data[720])
      .SetData361(static_cast<int64_t>(raw_data[723]) << 32 | raw_data[722])
      .SetData362(static_cast<int64_t>(raw_data[725]) << 32 | raw_data[724])
      .SetData363(static_cast<int64_t>(raw_data[727]) << 32 | raw_data[726])
      .SetData364(static_cast<int64_t>(raw_data[729]) << 32 | raw_data[728])
      .SetData365(static_cast<int64_t>(raw_data[731]) << 32 | raw_data[730])
      .SetData366(static_cast<int64_t>(raw_data[733]) << 32 | raw_data[732])
      .SetData367(static_cast<int64_t>(raw_data[735]) << 32 | raw_data[734])
      .SetData368(static_cast<int64_t>(raw_data[737]) << 32 | raw_data[736])
      .SetData369(static_cast<int64_t>(raw_data[739]) << 32 | raw_data[738])
      .SetData370(static_cast<int64_t>(raw_data[741]) << 32 | raw_data[740])
      .SetData371(static_cast<int64_t>(raw_data[743]) << 32 | raw_data[742])
      .SetData372(static_cast<int64_t>(raw_data[745]) << 32 | raw_data[744])
      .SetData373(static_cast<int64_t>(raw_data[747]) << 32 | raw_data[746])
      .SetData374(static_cast<int64_t>(raw_data[749]) << 32 | raw_data[748])
      .SetData375(static_cast<int64_t>(raw_data[751]) << 32 | raw_data[750])
      .SetData376(static_cast<int64_t>(raw_data[753]) << 32 | raw_data[752])
      .SetData377(static_cast<int64_t>(raw_data[755]) << 32 | raw_data[754])
      .SetData378(static_cast<int64_t>(raw_data[757]) << 32 | raw_data[756])
      .SetData379(static_cast<int64_t>(raw_data[759]) << 32 | raw_data[758])
      .SetData380(static_cast<int64_t>(raw_data[761]) << 32 | raw_data[760])
      .SetData381(static_cast<int64_t>(raw_data[763]) << 32 | raw_data[762])
      .SetData382(static_cast<int64_t>(raw_data[765]) << 32 | raw_data[764])
      .SetData383(static_cast<int64_t>(raw_data[767]) << 32 | raw_data[766])
      .SetData384(static_cast<int64_t>(raw_data[769]) << 32 | raw_data[768])
      .SetData385(static_cast<int64_t>(raw_data[771]) << 32 | raw_data[770])
      .SetData386(static_cast<int64_t>(raw_data[773]) << 32 | raw_data[772])
      .SetData387(static_cast<int64_t>(raw_data[775]) << 32 | raw_data[774])
      .SetData388(static_cast<int64_t>(raw_data[777]) << 32 | raw_data[776])
      .SetData389(static_cast<int64_t>(raw_data[779]) << 32 | raw_data[778])
      .SetData390(static_cast<int64_t>(raw_data[781]) << 32 | raw_data[780])
      .SetData391(static_cast<int64_t>(raw_data[783]) << 32 | raw_data[782])
      .SetData392(static_cast<int64_t>(raw_data[785]) << 32 | raw_data[784])
      .SetData393(static_cast<int64_t>(raw_data[787]) << 32 | raw_data[786])
      .SetData394(static_cast<int64_t>(raw_data[789]) << 32 | raw_data[788])
      .SetData395(static_cast<int64_t>(raw_data[791]) << 32 | raw_data[790])
      .SetData396(static_cast<int64_t>(raw_data[793]) << 32 | raw_data[792])
      .SetData397(static_cast<int64_t>(raw_data[795]) << 32 | raw_data[794])
      .SetData398(static_cast<int64_t>(raw_data[797]) << 32 | raw_data[796])
      .SetData399(static_cast<int64_t>(raw_data[799]) << 32 | raw_data[798])
      .SetData400(static_cast<int64_t>(raw_data[801]) << 32 | raw_data[800])
      .SetData401(static_cast<int64_t>(raw_data[803]) << 32 | raw_data[802])
      .SetData402(static_cast<int64_t>(raw_data[805]) << 32 | raw_data[804])
      .SetData403(static_cast<int64_t>(raw_data[807]) << 32 | raw_data[806])
      .SetData404(static_cast<int64_t>(raw_data[809]) << 32 | raw_data[808])
      .SetData405(static_cast<int64_t>(raw_data[811]) << 32 | raw_data[810])
      .SetData406(static_cast<int64_t>(raw_data[813]) << 32 | raw_data[812])
      .SetData407(static_cast<int64_t>(raw_data[815]) << 32 | raw_data[814])
      .SetData408(static_cast<int64_t>(raw_data[817]) << 32 | raw_data[816])
      .SetData409(static_cast<int64_t>(raw_data[819]) << 32 | raw_data[818])
      .SetData410(static_cast<int64_t>(raw_data[821]) << 32 | raw_data[820])
      .SetData411(static_cast<int64_t>(raw_data[823]) << 32 | raw_data[822])
      .SetData412(static_cast<int64_t>(raw_data[825]) << 32 | raw_data[824])
      .SetData413(static_cast<int64_t>(raw_data[827]) << 32 | raw_data[826])
      .SetData414(static_cast<int64_t>(raw_data[829]) << 32 | raw_data[828])
      .SetData415(static_cast<int64_t>(raw_data[831]) << 32 | raw_data[830])
      .SetData416(static_cast<int64_t>(raw_data[833]) << 32 | raw_data[832])
      .SetData417(static_cast<int64_t>(raw_data[835]) << 32 | raw_data[834])
      .SetData418(static_cast<int64_t>(raw_data[837]) << 32 | raw_data[836])
      .SetData419(static_cast<int64_t>(raw_data[839]) << 32 | raw_data[838])
      .SetData420(static_cast<int64_t>(raw_data[841]) << 32 | raw_data[840])
      .SetData421(static_cast<int64_t>(raw_data[843]) << 32 | raw_data[842])
      .SetData422(static_cast<int64_t>(raw_data[845]) << 32 | raw_data[844])
      .SetData423(static_cast<int64_t>(raw_data[847]) << 32 | raw_data[846])
      .SetData424(static_cast<int64_t>(raw_data[849]) << 32 | raw_data[848])
      .SetData425(static_cast<int64_t>(raw_data[851]) << 32 | raw_data[850])
      .SetData426(static_cast<int64_t>(raw_data[853]) << 32 | raw_data[852])
      .SetData427(static_cast<int64_t>(raw_data[855]) << 32 | raw_data[854])
      .SetData428(static_cast<int64_t>(raw_data[857]) << 32 | raw_data[856])
      .SetData429(static_cast<int64_t>(raw_data[859]) << 32 | raw_data[858])
      .SetData430(static_cast<int64_t>(raw_data[861]) << 32 | raw_data[860])
      .SetData431(static_cast<int64_t>(raw_data[863]) << 32 | raw_data[862])
      .SetData432(static_cast<int64_t>(raw_data[865]) << 32 | raw_data[864])
      .SetData433(static_cast<int64_t>(raw_data[867]) << 32 | raw_data[866])
      .SetData434(static_cast<int64_t>(raw_data[869]) << 32 | raw_data[868])
      .SetData435(static_cast<int64_t>(raw_data[871]) << 32 | raw_data[870])
      .SetData436(static_cast<int64_t>(raw_data[873]) << 32 | raw_data[872])
      .SetData437(static_cast<int64_t>(raw_data[875]) << 32 | raw_data[874])
      .SetData438(static_cast<int64_t>(raw_data[877]) << 32 | raw_data[876])
      .SetData439(static_cast<int64_t>(raw_data[879]) << 32 | raw_data[878])
      .SetData440(static_cast<int64_t>(raw_data[881]) << 32 | raw_data[880])
      .SetData441(static_cast<int64_t>(raw_data[883]) << 32 | raw_data[882])
      .SetData442(static_cast<int64_t>(raw_data[885]) << 32 | raw_data[884])
      .SetData443(static_cast<int64_t>(raw_data[887]) << 32 | raw_data[886])
      .SetData444(static_cast<int64_t>(raw_data[889]) << 32 | raw_data[888])
      .SetData445(static_cast<int64_t>(raw_data[891]) << 32 | raw_data[890])
      .SetData446(static_cast<int64_t>(raw_data[893]) << 32 | raw_data[892])
      .SetData447(static_cast<int64_t>(raw_data[895]) << 32 | raw_data[894])
      .SetData448(static_cast<int64_t>(raw_data[897]) << 32 | raw_data[896])
      .SetData449(static_cast<int64_t>(raw_data[899]) << 32 | raw_data[898])
      .SetData450(static_cast<int64_t>(raw_data[901]) << 32 | raw_data[900])
      .SetData451(static_cast<int64_t>(raw_data[903]) << 32 | raw_data[902])
      .SetData452(static_cast<int64_t>(raw_data[905]) << 32 | raw_data[904])
      .SetData453(static_cast<int64_t>(raw_data[907]) << 32 | raw_data[906])
      .SetData454(static_cast<int64_t>(raw_data[909]) << 32 | raw_data[908])
      .SetData455(static_cast<int64_t>(raw_data[911]) << 32 | raw_data[910])
      .SetData456(static_cast<int64_t>(raw_data[913]) << 32 | raw_data[912])
      .SetData457(static_cast<int64_t>(raw_data[915]) << 32 | raw_data[914])
      .SetData458(static_cast<int64_t>(raw_data[917]) << 32 | raw_data[916])
      .SetData459(static_cast<int64_t>(raw_data[919]) << 32 | raw_data[918])
      .SetData460(static_cast<int64_t>(raw_data[921]) << 32 | raw_data[920])
      .SetData461(static_cast<int64_t>(raw_data[923]) << 32 | raw_data[922])
      .SetData462(static_cast<int64_t>(raw_data[925]) << 32 | raw_data[924])
      .SetData463(static_cast<int64_t>(raw_data[927]) << 32 | raw_data[926])
      .SetData464(static_cast<int64_t>(raw_data[929]) << 32 | raw_data[928])
      .SetData465(static_cast<int64_t>(raw_data[931]) << 32 | raw_data[930])
      .SetData466(static_cast<int64_t>(raw_data[933]) << 32 | raw_data[932])
      .SetData467(static_cast<int64_t>(raw_data[935]) << 32 | raw_data[934])
      .SetData468(static_cast<int64_t>(raw_data[937]) << 32 | raw_data[936])
      .SetData469(static_cast<int64_t>(raw_data[939]) << 32 | raw_data[938])
      .SetData470(static_cast<int64_t>(raw_data[941]) << 32 | raw_data[940])
      .SetData471(static_cast<int64_t>(raw_data[943]) << 32 | raw_data[942])
      .SetData472(static_cast<int64_t>(raw_data[945]) << 32 | raw_data[944])
      .SetData473(static_cast<int64_t>(raw_data[947]) << 32 | raw_data[946])
      .SetData474(static_cast<int64_t>(raw_data[949]) << 32 | raw_data[948])
      .SetData475(static_cast<int64_t>(raw_data[951]) << 32 | raw_data[950])
      .SetData476(static_cast<int64_t>(raw_data[953]) << 32 | raw_data[952])
      .SetData477(static_cast<int64_t>(raw_data[955]) << 32 | raw_data[954])
      .SetData478(static_cast<int64_t>(raw_data[957]) << 32 | raw_data[956])
      .SetData479(static_cast<int64_t>(raw_data[959]) << 32 | raw_data[958])
      .SetData480(static_cast<int64_t>(raw_data[961]) << 32 | raw_data[960])
      .SetData481(static_cast<int64_t>(raw_data[963]) << 32 | raw_data[962])
      .SetData482(static_cast<int64_t>(raw_data[965]) << 32 | raw_data[964])
      .SetData483(static_cast<int64_t>(raw_data[967]) << 32 | raw_data[966])
      .SetData484(static_cast<int64_t>(raw_data[969]) << 32 | raw_data[968])
      .SetData485(static_cast<int64_t>(raw_data[971]) << 32 | raw_data[970])
      .SetData486(static_cast<int64_t>(raw_data[973]) << 32 | raw_data[972])
      .SetData487(static_cast<int64_t>(raw_data[975]) << 32 | raw_data[974])
      .SetData488(static_cast<int64_t>(raw_data[977]) << 32 | raw_data[976])
      .SetData489(static_cast<int64_t>(raw_data[979]) << 32 | raw_data[978])
      .SetData490(static_cast<int64_t>(raw_data[981]) << 32 | raw_data[980])
      .SetData491(static_cast<int64_t>(raw_data[983]) << 32 | raw_data[982])
      .SetData492(static_cast<int64_t>(raw_data[985]) << 32 | raw_data[984])
      .SetData493(static_cast<int64_t>(raw_data[987]) << 32 | raw_data[986])
      .SetData494(static_cast<int64_t>(raw_data[989]) << 32 | raw_data[988])
      .SetData495(static_cast<int64_t>(raw_data[991]) << 32 | raw_data[990])
      .SetData496(static_cast<int64_t>(raw_data[993]) << 32 | raw_data[992])
      .SetData497(static_cast<int64_t>(raw_data[995]) << 32 | raw_data[994])
      .SetData498(static_cast<int64_t>(raw_data[997]) << 32 | raw_data[996])
      .SetData499(static_cast<int64_t>(raw_data[999]) << 32 | raw_data[998])
      .SetData500(static_cast<int64_t>(raw_data[1001]) << 32 | raw_data[1000])
      .SetData501(static_cast<int64_t>(raw_data[1003]) << 32 | raw_data[1002])
      .SetData502(static_cast<int64_t>(raw_data[1005]) << 32 | raw_data[1004])
      .SetData503(static_cast<int64_t>(raw_data[1007]) << 32 | raw_data[1006])
      .SetData504(static_cast<int64_t>(raw_data[1009]) << 32 | raw_data[1008])
      .SetData505(static_cast<int64_t>(raw_data[1011]) << 32 | raw_data[1010])
      .SetData506(static_cast<int64_t>(raw_data[1013]) << 32 | raw_data[1012])
      .SetData507(static_cast<int64_t>(raw_data[1015]) << 32 | raw_data[1014])
      .SetData508(static_cast<int64_t>(raw_data[1017]) << 32 | raw_data[1016])
      .SetData509(static_cast<int64_t>(raw_data[1019]) << 32 | raw_data[1018])
      .SetData510(static_cast<int64_t>(raw_data[1021]) << 32 | raw_data[1020])
      .SetData511(static_cast<int64_t>(raw_data[1023]) << 32 | raw_data[1022])
      .SetData512(static_cast<int64_t>(raw_data[1025]) << 32 | raw_data[1024])
      .SetData513(static_cast<int64_t>(raw_data[1027]) << 32 | raw_data[1026])
      .SetData514(static_cast<int64_t>(raw_data[1029]) << 32 | raw_data[1028])
      .SetData515(static_cast<int64_t>(raw_data[1031]) << 32 | raw_data[1030])
      .SetData516(static_cast<int64_t>(raw_data[1033]) << 32 | raw_data[1032])
      .SetData517(static_cast<int64_t>(raw_data[1035]) << 32 | raw_data[1034])
      .SetData518(static_cast<int64_t>(raw_data[1037]) << 32 | raw_data[1036])
      .SetData519(static_cast<int64_t>(raw_data[1039]) << 32 | raw_data[1038])
      .SetData520(static_cast<int64_t>(raw_data[1041]) << 32 | raw_data[1040])
      .SetData521(static_cast<int64_t>(raw_data[1043]) << 32 | raw_data[1042])
      .SetData522(static_cast<int64_t>(raw_data[1045]) << 32 | raw_data[1044])
      .SetData523(static_cast<int64_t>(raw_data[1047]) << 32 | raw_data[1046])
      .SetData524(static_cast<int64_t>(raw_data[1049]) << 32 | raw_data[1048])
      .SetData525(static_cast<int64_t>(raw_data[1051]) << 32 | raw_data[1050])
      .SetData526(static_cast<int64_t>(raw_data[1053]) << 32 | raw_data[1052])
      .SetData527(static_cast<int64_t>(raw_data[1055]) << 32 | raw_data[1054])
      .SetData528(static_cast<int64_t>(raw_data[1057]) << 32 | raw_data[1056])
      .SetData529(static_cast<int64_t>(raw_data[1059]) << 32 | raw_data[1058])
      .SetData530(static_cast<int64_t>(raw_data[1061]) << 32 | raw_data[1060])
      .SetData531(static_cast<int64_t>(raw_data[1063]) << 32 | raw_data[1062])
      .SetData532(static_cast<int64_t>(raw_data[1065]) << 32 | raw_data[1064])
      .SetData533(static_cast<int64_t>(raw_data[1067]) << 32 | raw_data[1066])
      .SetData534(static_cast<int64_t>(raw_data[1069]) << 32 | raw_data[1068])
      .SetData535(static_cast<int64_t>(raw_data[1071]) << 32 | raw_data[1070])
      .SetData536(static_cast<int64_t>(raw_data[1073]) << 32 | raw_data[1072])
      .SetData537(static_cast<int64_t>(raw_data[1075]) << 32 | raw_data[1074])
      .SetData538(static_cast<int64_t>(raw_data[1077]) << 32 | raw_data[1076])
      .SetData539(static_cast<int64_t>(raw_data[1079]) << 32 | raw_data[1078])
      .SetData540(static_cast<int64_t>(raw_data[1081]) << 32 | raw_data[1080])
      .SetData541(static_cast<int64_t>(raw_data[1083]) << 32 | raw_data[1082])
      .SetData542(static_cast<int64_t>(raw_data[1085]) << 32 | raw_data[1084])
      .SetData543(static_cast<int64_t>(raw_data[1087]) << 32 | raw_data[1086])
      .SetData544(static_cast<int64_t>(raw_data[1089]) << 32 | raw_data[1088])
      .SetData545(static_cast<int64_t>(raw_data[1091]) << 32 | raw_data[1090])
      .SetData546(static_cast<int64_t>(raw_data[1093]) << 32 | raw_data[1092])
      .SetData547(static_cast<int64_t>(raw_data[1095]) << 32 | raw_data[1094])
      .SetData548(static_cast<int64_t>(raw_data[1097]) << 32 | raw_data[1096])
      .SetData549(static_cast<int64_t>(raw_data[1099]) << 32 | raw_data[1098])
      .SetData550(static_cast<int64_t>(raw_data[1101]) << 32 | raw_data[1100])
      .SetData551(static_cast<int64_t>(raw_data[1103]) << 32 | raw_data[1102])
      .SetData552(static_cast<int64_t>(raw_data[1105]) << 32 | raw_data[1104])
      .SetData553(static_cast<int64_t>(raw_data[1107]) << 32 | raw_data[1106])
      .SetData554(static_cast<int64_t>(raw_data[1109]) << 32 | raw_data[1108])
      .SetData555(static_cast<int64_t>(raw_data[1111]) << 32 | raw_data[1110])
      .SetData556(static_cast<int64_t>(raw_data[1113]) << 32 | raw_data[1112])
      .SetData557(static_cast<int64_t>(raw_data[1115]) << 32 | raw_data[1114])
      .SetData558(static_cast<int64_t>(raw_data[1117]) << 32 | raw_data[1116])
      .SetData559(static_cast<int64_t>(raw_data[1119]) << 32 | raw_data[1118])
      .SetData560(static_cast<int64_t>(raw_data[1121]) << 32 | raw_data[1120])
      .SetData561(static_cast<int64_t>(raw_data[1123]) << 32 | raw_data[1122])
      .SetData562(static_cast<int64_t>(raw_data[1125]) << 32 | raw_data[1124])
      .SetData563(static_cast<int64_t>(raw_data[1127]) << 32 | raw_data[1126])
      .SetData564(static_cast<int64_t>(raw_data[1129]) << 32 | raw_data[1128])
      .SetData565(static_cast<int64_t>(raw_data[1131]) << 32 | raw_data[1130])
      .SetData566(static_cast<int64_t>(raw_data[1133]) << 32 | raw_data[1132])
      .SetData567(static_cast<int64_t>(raw_data[1135]) << 32 | raw_data[1134])
      .SetData568(static_cast<int64_t>(raw_data[1137]) << 32 | raw_data[1136])
      .SetData569(static_cast<int64_t>(raw_data[1139]) << 32 | raw_data[1138])
      .SetData570(static_cast<int64_t>(raw_data[1141]) << 32 | raw_data[1140])
      .SetData571(static_cast<int64_t>(raw_data[1143]) << 32 | raw_data[1142])
      .SetData572(static_cast<int64_t>(raw_data[1145]) << 32 | raw_data[1144])
      .SetData573(static_cast<int64_t>(raw_data[1147]) << 32 | raw_data[1146])
      .SetData574(static_cast<int64_t>(raw_data[1149]) << 32 | raw_data[1148])
      .SetData575(static_cast<int64_t>(raw_data[1151]) << 32 | raw_data[1150])
      .SetData576(static_cast<int64_t>(raw_data[1153]) << 32 | raw_data[1152])
      .SetData577(static_cast<int64_t>(raw_data[1155]) << 32 | raw_data[1154])
      .SetData578(static_cast<int64_t>(raw_data[1157]) << 32 | raw_data[1156])
      .SetData579(static_cast<int64_t>(raw_data[1159]) << 32 | raw_data[1158])
      .SetData580(static_cast<int64_t>(raw_data[1161]) << 32 | raw_data[1160])
      .SetData581(static_cast<int64_t>(raw_data[1163]) << 32 | raw_data[1162])
      .SetData582(static_cast<int64_t>(raw_data[1165]) << 32 | raw_data[1164])
      .SetData583(static_cast<int64_t>(raw_data[1167]) << 32 | raw_data[1166])
      .SetData584(static_cast<int64_t>(raw_data[1169]) << 32 | raw_data[1168])
      .SetData585(static_cast<int64_t>(raw_data[1171]) << 32 | raw_data[1170])
      .SetData586(static_cast<int64_t>(raw_data[1173]) << 32 | raw_data[1172])
      .SetData587(static_cast<int64_t>(raw_data[1175]) << 32 | raw_data[1174])
      .SetData588(static_cast<int64_t>(raw_data[1177]) << 32 | raw_data[1176])
      .SetData589(static_cast<int64_t>(raw_data[1179]) << 32 | raw_data[1178])
      .SetData590(static_cast<int64_t>(raw_data[1181]) << 32 | raw_data[1180])
      .SetData591(static_cast<int64_t>(raw_data[1183]) << 32 | raw_data[1182])
      .SetData592(static_cast<int64_t>(raw_data[1185]) << 32 | raw_data[1184])
      .SetData593(static_cast<int64_t>(raw_data[1187]) << 32 | raw_data[1186])
      .SetData594(static_cast<int64_t>(raw_data[1189]) << 32 | raw_data[1188])
      .SetData595(static_cast<int64_t>(raw_data[1191]) << 32 | raw_data[1190])
      .SetData596(static_cast<int64_t>(raw_data[1193]) << 32 | raw_data[1192])
      .SetData597(static_cast<int64_t>(raw_data[1195]) << 32 | raw_data[1194])
      .SetData598(static_cast<int64_t>(raw_data[1197]) << 32 | raw_data[1196])
      .SetData599(static_cast<int64_t>(raw_data[1199]) << 32 | raw_data[1198])
      .SetData600(static_cast<int64_t>(raw_data[1201]) << 32 | raw_data[1200])
      .SetData601(static_cast<int64_t>(raw_data[1203]) << 32 | raw_data[1202])
      .SetData602(static_cast<int64_t>(raw_data[1205]) << 32 | raw_data[1204])
      .SetData603(static_cast<int64_t>(raw_data[1207]) << 32 | raw_data[1206])
      .SetData604(static_cast<int64_t>(raw_data[1209]) << 32 | raw_data[1208])
      .SetData605(static_cast<int64_t>(raw_data[1211]) << 32 | raw_data[1210])
      .SetData606(static_cast<int64_t>(raw_data[1213]) << 32 | raw_data[1212])
      .SetData607(static_cast<int64_t>(raw_data[1215]) << 32 | raw_data[1214])
      .SetData608(static_cast<int64_t>(raw_data[1217]) << 32 | raw_data[1216])
      .SetData609(static_cast<int64_t>(raw_data[1219]) << 32 | raw_data[1218])
      .SetData610(static_cast<int64_t>(raw_data[1221]) << 32 | raw_data[1220])
      .SetData611(static_cast<int64_t>(raw_data[1223]) << 32 | raw_data[1222])
      .SetData612(static_cast<int64_t>(raw_data[1225]) << 32 | raw_data[1224])
      .SetData613(static_cast<int64_t>(raw_data[1227]) << 32 | raw_data[1226])
      .SetData614(static_cast<int64_t>(raw_data[1229]) << 32 | raw_data[1228])
      .SetData615(static_cast<int64_t>(raw_data[1231]) << 32 | raw_data[1230])
      .SetData616(static_cast<int64_t>(raw_data[1233]) << 32 | raw_data[1232])
      .SetData617(static_cast<int64_t>(raw_data[1235]) << 32 | raw_data[1234])
      .SetData618(static_cast<int64_t>(raw_data[1237]) << 32 | raw_data[1236])
      .SetData619(static_cast<int64_t>(raw_data[1239]) << 32 | raw_data[1238])
      .SetData620(static_cast<int64_t>(raw_data[1241]) << 32 | raw_data[1240])
      .SetData621(static_cast<int64_t>(raw_data[1243]) << 32 | raw_data[1242])
      .SetData622(static_cast<int64_t>(raw_data[1245]) << 32 | raw_data[1244])
      .SetData623(static_cast<int64_t>(raw_data[1247]) << 32 | raw_data[1246])
      .SetData624(static_cast<int64_t>(raw_data[1249]) << 32 | raw_data[1248])
      .SetData625(static_cast<int64_t>(raw_data[1251]) << 32 | raw_data[1250])
      .SetData626(static_cast<int64_t>(raw_data[1253]) << 32 | raw_data[1252])
      .SetData627(static_cast<int64_t>(raw_data[1255]) << 32 | raw_data[1254])
      .SetData628(static_cast<int64_t>(raw_data[1257]) << 32 | raw_data[1256])
      .SetData629(static_cast<int64_t>(raw_data[1259]) << 32 | raw_data[1258])
      .SetData630(static_cast<int64_t>(raw_data[1261]) << 32 | raw_data[1260])
      .SetData631(static_cast<int64_t>(raw_data[1263]) << 32 | raw_data[1262])
      .SetData632(static_cast<int64_t>(raw_data[1265]) << 32 | raw_data[1264])
      .SetData633(static_cast<int64_t>(raw_data[1267]) << 32 | raw_data[1266])
      .SetData634(static_cast<int64_t>(raw_data[1269]) << 32 | raw_data[1268])
      .SetData635(static_cast<int64_t>(raw_data[1271]) << 32 | raw_data[1270])
      .SetData636(static_cast<int64_t>(raw_data[1273]) << 32 | raw_data[1272])
      .SetData637(static_cast<int64_t>(raw_data[1275]) << 32 | raw_data[1274])
      .SetData638(static_cast<int64_t>(raw_data[1277]) << 32 | raw_data[1276])
      .SetData639(static_cast<int64_t>(raw_data[1279]) << 32 | raw_data[1278])
      .SetData640(static_cast<int64_t>(raw_data[1281]) << 32 | raw_data[1280])
      .SetData641(static_cast<int64_t>(raw_data[1283]) << 32 | raw_data[1282])
      .SetData642(static_cast<int64_t>(raw_data[1285]) << 32 | raw_data[1284])
      .SetData643(static_cast<int64_t>(raw_data[1287]) << 32 | raw_data[1286])
      .SetData644(static_cast<int64_t>(raw_data[1289]) << 32 | raw_data[1288])
      .SetData645(static_cast<int64_t>(raw_data[1291]) << 32 | raw_data[1290])
      .SetData646(static_cast<int64_t>(raw_data[1293]) << 32 | raw_data[1292])
      .SetData647(static_cast<int64_t>(raw_data[1295]) << 32 | raw_data[1294])
      .SetData648(static_cast<int64_t>(raw_data[1297]) << 32 | raw_data[1296])
      .SetData649(static_cast<int64_t>(raw_data[1299]) << 32 | raw_data[1298])
      .SetData650(static_cast<int64_t>(raw_data[1301]) << 32 | raw_data[1300])
      .SetData651(static_cast<int64_t>(raw_data[1303]) << 32 | raw_data[1302])
      .SetData652(static_cast<int64_t>(raw_data[1305]) << 32 | raw_data[1304])
      .SetData653(static_cast<int64_t>(raw_data[1307]) << 32 | raw_data[1306])
      .SetData654(static_cast<int64_t>(raw_data[1309]) << 32 | raw_data[1308])
      .SetData655(static_cast<int64_t>(raw_data[1311]) << 32 | raw_data[1310])
      .SetData656(static_cast<int64_t>(raw_data[1313]) << 32 | raw_data[1312])
      .SetData657(static_cast<int64_t>(raw_data[1315]) << 32 | raw_data[1314])
      .SetData658(static_cast<int64_t>(raw_data[1317]) << 32 | raw_data[1316])
      .SetData659(static_cast<int64_t>(raw_data[1319]) << 32 | raw_data[1318])
      .SetData660(static_cast<int64_t>(raw_data[1321]) << 32 | raw_data[1320])
      .SetData661(static_cast<int64_t>(raw_data[1323]) << 32 | raw_data[1322])
      .SetData662(static_cast<int64_t>(raw_data[1325]) << 32 | raw_data[1324])
      .SetData663(static_cast<int64_t>(raw_data[1327]) << 32 | raw_data[1326])
      .SetData664(static_cast<int64_t>(raw_data[1329]) << 32 | raw_data[1328])
      .SetData665(static_cast<int64_t>(raw_data[1331]) << 32 | raw_data[1330])
      .SetData666(static_cast<int64_t>(raw_data[1333]) << 32 | raw_data[1332])
      .SetData667(static_cast<int64_t>(raw_data[1335]) << 32 | raw_data[1334])
      .SetData668(static_cast<int64_t>(raw_data[1337]) << 32 | raw_data[1336])
      .SetData669(static_cast<int64_t>(raw_data[1339]) << 32 | raw_data[1338])
      .SetData670(static_cast<int64_t>(raw_data[1341]) << 32 | raw_data[1340])
      .SetData671(static_cast<int64_t>(raw_data[1343]) << 32 | raw_data[1342])
      .SetData672(static_cast<int64_t>(raw_data[1345]) << 32 | raw_data[1344])
      .SetData673(static_cast<int64_t>(raw_data[1347]) << 32 | raw_data[1346])
      .SetData674(static_cast<int64_t>(raw_data[1349]) << 32 | raw_data[1348])
      .SetData675(static_cast<int64_t>(raw_data[1351]) << 32 | raw_data[1350])
      .SetData676(static_cast<int64_t>(raw_data[1353]) << 32 | raw_data[1352])
      .SetData677(static_cast<int64_t>(raw_data[1355]) << 32 | raw_data[1354])
      .SetData678(static_cast<int64_t>(raw_data[1357]) << 32 | raw_data[1356])
      .SetData679(static_cast<int64_t>(raw_data[1359]) << 32 | raw_data[1358])
      .SetData680(static_cast<int64_t>(raw_data[1361]) << 32 | raw_data[1360])
      .SetData681(static_cast<int64_t>(raw_data[1363]) << 32 | raw_data[1362])
      .SetData682(static_cast<int64_t>(raw_data[1365]) << 32 | raw_data[1364])
      .SetData683(static_cast<int64_t>(raw_data[1367]) << 32 | raw_data[1366])
      .SetData684(static_cast<int64_t>(raw_data[1369]) << 32 | raw_data[1368])
      .SetData685(static_cast<int64_t>(raw_data[1371]) << 32 | raw_data[1370])
      .SetData686(static_cast<int64_t>(raw_data[1373]) << 32 | raw_data[1372])
      .SetData687(static_cast<int64_t>(raw_data[1375]) << 32 | raw_data[1374])
      .SetData688(static_cast<int64_t>(raw_data[1377]) << 32 | raw_data[1376])
      .SetData689(static_cast<int64_t>(raw_data[1379]) << 32 | raw_data[1378])
      .SetData690(static_cast<int64_t>(raw_data[1381]) << 32 | raw_data[1380])
      .SetData691(static_cast<int64_t>(raw_data[1383]) << 32 | raw_data[1382])
      .SetData692(static_cast<int64_t>(raw_data[1385]) << 32 | raw_data[1384])
      .SetData693(static_cast<int64_t>(raw_data[1387]) << 32 | raw_data[1386])
      .SetData694(static_cast<int64_t>(raw_data[1389]) << 32 | raw_data[1388])
      .SetData695(static_cast<int64_t>(raw_data[1391]) << 32 | raw_data[1390])
      .SetData696(static_cast<int64_t>(raw_data[1393]) << 32 | raw_data[1392])
      .SetData697(static_cast<int64_t>(raw_data[1395]) << 32 | raw_data[1394])
      .SetData698(static_cast<int64_t>(raw_data[1397]) << 32 | raw_data[1396])
      .SetData699(static_cast<int64_t>(raw_data[1399]) << 32 | raw_data[1398])
      .SetData700(static_cast<int64_t>(raw_data[1401]) << 32 | raw_data[1400])
      .SetData701(static_cast<int64_t>(raw_data[1403]) << 32 | raw_data[1402])
      .SetData702(static_cast<int64_t>(raw_data[1405]) << 32 | raw_data[1404])
      .SetData703(static_cast<int64_t>(raw_data[1407]) << 32 | raw_data[1406])
      .SetData704(static_cast<int64_t>(raw_data[1409]) << 32 | raw_data[1408])
      .SetData705(static_cast<int64_t>(raw_data[1411]) << 32 | raw_data[1410])
      .SetData706(static_cast<int64_t>(raw_data[1413]) << 32 | raw_data[1412])
      .SetData707(static_cast<int64_t>(raw_data[1415]) << 32 | raw_data[1414])
      .SetData708(static_cast<int64_t>(raw_data[1417]) << 32 | raw_data[1416])
      .SetData709(static_cast<int64_t>(raw_data[1419]) << 32 | raw_data[1418])
      .SetData710(static_cast<int64_t>(raw_data[1421]) << 32 | raw_data[1420])
      .SetData711(static_cast<int64_t>(raw_data[1423]) << 32 | raw_data[1422])
      .SetData712(static_cast<int64_t>(raw_data[1425]) << 32 | raw_data[1424])
      .SetData713(static_cast<int64_t>(raw_data[1427]) << 32 | raw_data[1426])
      .SetData714(static_cast<int64_t>(raw_data[1429]) << 32 | raw_data[1428])
      .SetData715(static_cast<int64_t>(raw_data[1431]) << 32 | raw_data[1430])
      .SetData716(static_cast<int64_t>(raw_data[1433]) << 32 | raw_data[1432])
      .SetData717(static_cast<int64_t>(raw_data[1435]) << 32 | raw_data[1434])
      .SetData718(static_cast<int64_t>(raw_data[1437]) << 32 | raw_data[1436])
      .SetData719(static_cast<int64_t>(raw_data[1439]) << 32 | raw_data[1438])
      .SetData720(static_cast<int64_t>(raw_data[1441]) << 32 | raw_data[1440])
      .SetData721(static_cast<int64_t>(raw_data[1443]) << 32 | raw_data[1442])
      .SetData722(static_cast<int64_t>(raw_data[1445]) << 32 | raw_data[1444])
      .SetData723(static_cast<int64_t>(raw_data[1447]) << 32 | raw_data[1446])
      .SetData724(static_cast<int64_t>(raw_data[1449]) << 32 | raw_data[1448])
      .SetData725(static_cast<int64_t>(raw_data[1451]) << 32 | raw_data[1450])
      .SetData726(static_cast<int64_t>(raw_data[1453]) << 32 | raw_data[1452])
      .SetData727(static_cast<int64_t>(raw_data[1455]) << 32 | raw_data[1454])
      .SetData728(static_cast<int64_t>(raw_data[1457]) << 32 | raw_data[1456])
      .SetData729(static_cast<int64_t>(raw_data[1459]) << 32 | raw_data[1458])
      .SetData730(static_cast<int64_t>(raw_data[1461]) << 32 | raw_data[1460])
      .SetData731(static_cast<int64_t>(raw_data[1463]) << 32 | raw_data[1462])
      .SetData732(static_cast<int64_t>(raw_data[1465]) << 32 | raw_data[1464])
      .SetData733(static_cast<int64_t>(raw_data[1467]) << 32 | raw_data[1466])
      .SetData734(static_cast<int64_t>(raw_data[1469]) << 32 | raw_data[1468])
      .SetData735(static_cast<int64_t>(raw_data[1471]) << 32 | raw_data[1470])
      .SetData736(static_cast<int64_t>(raw_data[1473]) << 32 | raw_data[1472])
      .SetData737(static_cast<int64_t>(raw_data[1475]) << 32 | raw_data[1474])
      .SetData738(static_cast<int64_t>(raw_data[1477]) << 32 | raw_data[1476])
      .SetData739(static_cast<int64_t>(raw_data[1479]) << 32 | raw_data[1478])
      .SetData740(static_cast<int64_t>(raw_data[1481]) << 32 | raw_data[1480])
      .SetData741(static_cast<int64_t>(raw_data[1483]) << 32 | raw_data[1482])
      .SetData742(static_cast<int64_t>(raw_data[1485]) << 32 | raw_data[1484])
      .SetData743(static_cast<int64_t>(raw_data[1487]) << 32 | raw_data[1486])
      .SetData744(static_cast<int64_t>(raw_data[1489]) << 32 | raw_data[1488])
      .SetData745(static_cast<int64_t>(raw_data[1491]) << 32 | raw_data[1490])
      .SetData746(static_cast<int64_t>(raw_data[1493]) << 32 | raw_data[1492])
      .SetData747(static_cast<int64_t>(raw_data[1495]) << 32 | raw_data[1494])
      .SetData748(static_cast<int64_t>(raw_data[1497]) << 32 | raw_data[1496])
      .SetData749(static_cast<int64_t>(raw_data[1499]) << 32 | raw_data[1498])
      .SetData750(static_cast<int64_t>(raw_data[1501]) << 32 | raw_data[1500])
      .SetData751(static_cast<int64_t>(raw_data[1503]) << 32 | raw_data[1502])
      .SetData752(static_cast<int64_t>(raw_data[1505]) << 32 | raw_data[1504])
      .SetData753(static_cast<int64_t>(raw_data[1507]) << 32 | raw_data[1506])
      .SetData754(static_cast<int64_t>(raw_data[1509]) << 32 | raw_data[1508])
      .SetData755(static_cast<int64_t>(raw_data[1511]) << 32 | raw_data[1510])
      .SetData756(static_cast<int64_t>(raw_data[1513]) << 32 | raw_data[1512])
      .SetData757(static_cast<int64_t>(raw_data[1515]) << 32 | raw_data[1514])
      .SetData758(static_cast<int64_t>(raw_data[1517]) << 32 | raw_data[1516])
      .SetData759(static_cast<int64_t>(raw_data[1519]) << 32 | raw_data[1518])
      .SetData760(static_cast<int64_t>(raw_data[1521]) << 32 | raw_data[1520])
      .SetData761(static_cast<int64_t>(raw_data[1523]) << 32 | raw_data[1522])
      .SetData762(static_cast<int64_t>(raw_data[1525]) << 32 | raw_data[1524])
      .SetData763(static_cast<int64_t>(raw_data[1527]) << 32 | raw_data[1526])
      .SetData764(static_cast<int64_t>(raw_data[1529]) << 32 | raw_data[1528])
      .SetData765(static_cast<int64_t>(raw_data[1531]) << 32 | raw_data[1530])
      .SetData766(static_cast<int64_t>(raw_data[1533]) << 32 | raw_data[1532])
      .SetData767(static_cast<int64_t>(raw_data[1535]) << 32 | raw_data[1534])
      .SetData768(static_cast<int64_t>(raw_data[1537]) << 32 | raw_data[1536])
      .SetData769(static_cast<int64_t>(raw_data[1539]) << 32 | raw_data[1538])
      .SetData770(static_cast<int64_t>(raw_data[1541]) << 32 | raw_data[1540])
      .SetData771(static_cast<int64_t>(raw_data[1543]) << 32 | raw_data[1542])
      .SetData772(static_cast<int64_t>(raw_data[1545]) << 32 | raw_data[1544])
      .SetData773(static_cast<int64_t>(raw_data[1547]) << 32 | raw_data[1546])
      .SetData774(static_cast<int64_t>(raw_data[1549]) << 32 | raw_data[1548])
      .SetData775(static_cast<int64_t>(raw_data[1551]) << 32 | raw_data[1550])
      .SetData776(static_cast<int64_t>(raw_data[1553]) << 32 | raw_data[1552])
      .SetData777(static_cast<int64_t>(raw_data[1555]) << 32 | raw_data[1554])
      .SetData778(static_cast<int64_t>(raw_data[1557]) << 32 | raw_data[1556])
      .SetData779(static_cast<int64_t>(raw_data[1559]) << 32 | raw_data[1558])
      .SetData780(static_cast<int64_t>(raw_data[1561]) << 32 | raw_data[1560])
      .SetData781(static_cast<int64_t>(raw_data[1563]) << 32 | raw_data[1562])
      .SetData782(static_cast<int64_t>(raw_data[1565]) << 32 | raw_data[1564])
      .SetData783(static_cast<int64_t>(raw_data[1567]) << 32 | raw_data[1566])
      .SetData784(static_cast<int64_t>(raw_data[1569]) << 32 | raw_data[1568])
      .SetData785(static_cast<int64_t>(raw_data[1571]) << 32 | raw_data[1570])
      .SetData786(static_cast<int64_t>(raw_data[1573]) << 32 | raw_data[1572])
      .SetData787(static_cast<int64_t>(raw_data[1575]) << 32 | raw_data[1574])
      .SetData788(static_cast<int64_t>(raw_data[1577]) << 32 | raw_data[1576])
      .SetData789(static_cast<int64_t>(raw_data[1579]) << 32 | raw_data[1578])
      .SetData790(static_cast<int64_t>(raw_data[1581]) << 32 | raw_data[1580])
      .SetData791(static_cast<int64_t>(raw_data[1583]) << 32 | raw_data[1582])
      .SetData792(static_cast<int64_t>(raw_data[1585]) << 32 | raw_data[1584])
      .SetData793(static_cast<int64_t>(raw_data[1587]) << 32 | raw_data[1586])
      .SetData794(static_cast<int64_t>(raw_data[1589]) << 32 | raw_data[1588])
      .SetData795(static_cast<int64_t>(raw_data[1591]) << 32 | raw_data[1590])
      .SetData796(static_cast<int64_t>(raw_data[1593]) << 32 | raw_data[1592])
      .SetData797(static_cast<int64_t>(raw_data[1595]) << 32 | raw_data[1594])
      .SetData798(static_cast<int64_t>(raw_data[1597]) << 32 | raw_data[1596])
      .SetData799(static_cast<int64_t>(raw_data[1599]) << 32 | raw_data[1598])
      .SetData800(static_cast<int64_t>(raw_data[1601]) << 32 | raw_data[1600])
      .SetData801(static_cast<int64_t>(raw_data[1603]) << 32 | raw_data[1602])
      .SetData802(static_cast<int64_t>(raw_data[1605]) << 32 | raw_data[1604])
      .SetData803(static_cast<int64_t>(raw_data[1607]) << 32 | raw_data[1606])
      .SetData804(static_cast<int64_t>(raw_data[1609]) << 32 | raw_data[1608])
      .SetData805(static_cast<int64_t>(raw_data[1611]) << 32 | raw_data[1610])
      .SetData806(static_cast<int64_t>(raw_data[1613]) << 32 | raw_data[1612])
      .SetData807(static_cast<int64_t>(raw_data[1615]) << 32 | raw_data[1614])
      .SetData808(static_cast<int64_t>(raw_data[1617]) << 32 | raw_data[1616])
      .SetData809(static_cast<int64_t>(raw_data[1619]) << 32 | raw_data[1618])
      .SetData810(static_cast<int64_t>(raw_data[1621]) << 32 | raw_data[1620])
      .SetData811(static_cast<int64_t>(raw_data[1623]) << 32 | raw_data[1622])
      .SetData812(static_cast<int64_t>(raw_data[1625]) << 32 | raw_data[1624])
      .SetData813(static_cast<int64_t>(raw_data[1627]) << 32 | raw_data[1626])
      .SetData814(static_cast<int64_t>(raw_data[1629]) << 32 | raw_data[1628])
      .SetData815(static_cast<int64_t>(raw_data[1631]) << 32 | raw_data[1630])
      .SetData816(static_cast<int64_t>(raw_data[1633]) << 32 | raw_data[1632])
      .SetData817(static_cast<int64_t>(raw_data[1635]) << 32 | raw_data[1634])
      .SetData818(static_cast<int64_t>(raw_data[1637]) << 32 | raw_data[1636])
      .SetData819(static_cast<int64_t>(raw_data[1639]) << 32 | raw_data[1638])
      .SetData820(static_cast<int64_t>(raw_data[1641]) << 32 | raw_data[1640])
      .SetData821(static_cast<int64_t>(raw_data[1643]) << 32 | raw_data[1642])
      .SetData822(static_cast<int64_t>(raw_data[1645]) << 32 | raw_data[1644])
      .SetData823(static_cast<int64_t>(raw_data[1647]) << 32 | raw_data[1646])
      .SetData824(static_cast<int64_t>(raw_data[1649]) << 32 | raw_data[1648])
      .SetData825(static_cast<int64_t>(raw_data[1651]) << 32 | raw_data[1650])
      .SetData826(static_cast<int64_t>(raw_data[1653]) << 32 | raw_data[1652])
      .SetData827(static_cast<int64_t>(raw_data[1655]) << 32 | raw_data[1654])
      .SetData828(static_cast<int64_t>(raw_data[1657]) << 32 | raw_data[1656])
      .SetData829(static_cast<int64_t>(raw_data[1659]) << 32 | raw_data[1658])
      .SetData830(static_cast<int64_t>(raw_data[1661]) << 32 | raw_data[1660])
      .SetData831(static_cast<int64_t>(raw_data[1663]) << 32 | raw_data[1662])
      .SetData832(static_cast<int64_t>(raw_data[1665]) << 32 | raw_data[1664])
      .SetData833(static_cast<int64_t>(raw_data[1667]) << 32 | raw_data[1666])
      .SetData834(static_cast<int64_t>(raw_data[1669]) << 32 | raw_data[1668])
      .SetData835(static_cast<int64_t>(raw_data[1671]) << 32 | raw_data[1670])
      .SetData836(static_cast<int64_t>(raw_data[1673]) << 32 | raw_data[1672])
      .SetData837(static_cast<int64_t>(raw_data[1675]) << 32 | raw_data[1674])
      .SetData838(static_cast<int64_t>(raw_data[1677]) << 32 | raw_data[1676])
      .SetData839(static_cast<int64_t>(raw_data[1679]) << 32 | raw_data[1678])
      .SetData840(static_cast<int64_t>(raw_data[1681]) << 32 | raw_data[1680])
      .SetData841(static_cast<int64_t>(raw_data[1683]) << 32 | raw_data[1682])
      .SetData842(static_cast<int64_t>(raw_data[1685]) << 32 | raw_data[1684])
      .SetData843(static_cast<int64_t>(raw_data[1687]) << 32 | raw_data[1686])
      .SetData844(static_cast<int64_t>(raw_data[1689]) << 32 | raw_data[1688])
      .SetData845(static_cast<int64_t>(raw_data[1691]) << 32 | raw_data[1690])
      .SetData846(static_cast<int64_t>(raw_data[1693]) << 32 | raw_data[1692])
      .SetData847(static_cast<int64_t>(raw_data[1695]) << 32 | raw_data[1694])
      .SetData848(static_cast<int64_t>(raw_data[1697]) << 32 | raw_data[1696])
      .SetData849(static_cast<int64_t>(raw_data[1699]) << 32 | raw_data[1698])
      .SetData850(static_cast<int64_t>(raw_data[1701]) << 32 | raw_data[1700])
      .SetData851(static_cast<int64_t>(raw_data[1703]) << 32 | raw_data[1702])
      .SetData852(static_cast<int64_t>(raw_data[1705]) << 32 | raw_data[1704])
      .SetData853(static_cast<int64_t>(raw_data[1707]) << 32 | raw_data[1706])
      .SetData854(static_cast<int64_t>(raw_data[1709]) << 32 | raw_data[1708])
      .SetData855(static_cast<int64_t>(raw_data[1711]) << 32 | raw_data[1710])
      .SetData856(static_cast<int64_t>(raw_data[1713]) << 32 | raw_data[1712])
      .SetData857(static_cast<int64_t>(raw_data[1715]) << 32 | raw_data[1714])
      .SetData858(static_cast<int64_t>(raw_data[1717]) << 32 | raw_data[1716])
      .SetData859(static_cast<int64_t>(raw_data[1719]) << 32 | raw_data[1718])
      .SetData860(static_cast<int64_t>(raw_data[1721]) << 32 | raw_data[1720])
      .SetData861(static_cast<int64_t>(raw_data[1723]) << 32 | raw_data[1722])
      .SetData862(static_cast<int64_t>(raw_data[1725]) << 32 | raw_data[1724])
      .SetData863(static_cast<int64_t>(raw_data[1727]) << 32 | raw_data[1726])
      .SetData864(static_cast<int64_t>(raw_data[1729]) << 32 | raw_data[1728])
      .SetData865(static_cast<int64_t>(raw_data[1731]) << 32 | raw_data[1730])
      .SetData866(static_cast<int64_t>(raw_data[1733]) << 32 | raw_data[1732])
      .SetData867(static_cast<int64_t>(raw_data[1735]) << 32 | raw_data[1734])
      .SetData868(static_cast<int64_t>(raw_data[1737]) << 32 | raw_data[1736])
      .SetData869(static_cast<int64_t>(raw_data[1739]) << 32 | raw_data[1738])
      .SetData870(static_cast<int64_t>(raw_data[1741]) << 32 | raw_data[1740])
      .SetData871(static_cast<int64_t>(raw_data[1743]) << 32 | raw_data[1742])
      .SetData872(static_cast<int64_t>(raw_data[1745]) << 32 | raw_data[1744])
      .SetData873(static_cast<int64_t>(raw_data[1747]) << 32 | raw_data[1746])
      .SetData874(static_cast<int64_t>(raw_data[1749]) << 32 | raw_data[1748])
      .SetData875(static_cast<int64_t>(raw_data[1751]) << 32 | raw_data[1750])
      .SetData876(static_cast<int64_t>(raw_data[1753]) << 32 | raw_data[1752])
      .SetData877(static_cast<int64_t>(raw_data[1755]) << 32 | raw_data[1754])
      .SetData878(static_cast<int64_t>(raw_data[1757]) << 32 | raw_data[1756])
      .SetData879(static_cast<int64_t>(raw_data[1759]) << 32 | raw_data[1758])
      .SetData880(static_cast<int64_t>(raw_data[1761]) << 32 | raw_data[1760])
      .SetData881(static_cast<int64_t>(raw_data[1763]) << 32 | raw_data[1762])
      .SetData882(static_cast<int64_t>(raw_data[1765]) << 32 | raw_data[1764])
      .SetData883(static_cast<int64_t>(raw_data[1767]) << 32 | raw_data[1766])
      .SetData884(static_cast<int64_t>(raw_data[1769]) << 32 | raw_data[1768])
      .SetData885(static_cast<int64_t>(raw_data[1771]) << 32 | raw_data[1770])
      .SetData886(static_cast<int64_t>(raw_data[1773]) << 32 | raw_data[1772])
      .SetData887(static_cast<int64_t>(raw_data[1775]) << 32 | raw_data[1774])
      .SetData888(static_cast<int64_t>(raw_data[1777]) << 32 | raw_data[1776])
      .SetData889(static_cast<int64_t>(raw_data[1779]) << 32 | raw_data[1778])
      .SetData890(static_cast<int64_t>(raw_data[1781]) << 32 | raw_data[1780])
      .SetData891(static_cast<int64_t>(raw_data[1783]) << 32 | raw_data[1782])
      .SetData892(static_cast<int64_t>(raw_data[1785]) << 32 | raw_data[1784])
      .SetData893(static_cast<int64_t>(raw_data[1787]) << 32 | raw_data[1786])
      .SetData894(static_cast<int64_t>(raw_data[1789]) << 32 | raw_data[1788])
      .SetData895(static_cast<int64_t>(raw_data[1791]) << 32 | raw_data[1790])
      .SetData896(static_cast<int64_t>(raw_data[1793]) << 32 | raw_data[1792])
      .SetData897(static_cast<int64_t>(raw_data[1795]) << 32 | raw_data[1794])
      .SetData898(static_cast<int64_t>(raw_data[1797]) << 32 | raw_data[1796])
      .SetData899(static_cast<int64_t>(raw_data[1799]) << 32 | raw_data[1798])
      .SetData900(static_cast<int64_t>(raw_data[1801]) << 32 | raw_data[1800])
      .SetData901(static_cast<int64_t>(raw_data[1803]) << 32 | raw_data[1802])
      .SetData902(static_cast<int64_t>(raw_data[1805]) << 32 | raw_data[1804])
      .SetData903(static_cast<int64_t>(raw_data[1807]) << 32 | raw_data[1806])
      .SetData904(static_cast<int64_t>(raw_data[1809]) << 32 | raw_data[1808])
      .SetData905(static_cast<int64_t>(raw_data[1811]) << 32 | raw_data[1810])
      .SetData906(static_cast<int64_t>(raw_data[1813]) << 32 | raw_data[1812])
      .SetData907(static_cast<int64_t>(raw_data[1815]) << 32 | raw_data[1814])
      .SetData908(static_cast<int64_t>(raw_data[1817]) << 32 | raw_data[1816])
      .SetData909(static_cast<int64_t>(raw_data[1819]) << 32 | raw_data[1818])
      .SetData910(static_cast<int64_t>(raw_data[1821]) << 32 | raw_data[1820])
      .SetData911(static_cast<int64_t>(raw_data[1823]) << 32 | raw_data[1822])
      .SetData912(static_cast<int64_t>(raw_data[1825]) << 32 | raw_data[1824])
      .SetData913(static_cast<int64_t>(raw_data[1827]) << 32 | raw_data[1826])
      .SetData914(static_cast<int64_t>(raw_data[1829]) << 32 | raw_data[1828])
      .SetData915(static_cast<int64_t>(raw_data[1831]) << 32 | raw_data[1830])
      .SetData916(static_cast<int64_t>(raw_data[1833]) << 32 | raw_data[1832])
      .SetData917(static_cast<int64_t>(raw_data[1835]) << 32 | raw_data[1834])
      .SetData918(static_cast<int64_t>(raw_data[1837]) << 32 | raw_data[1836])
      .SetData919(static_cast<int64_t>(raw_data[1839]) << 32 | raw_data[1838])
      .SetData920(static_cast<int64_t>(raw_data[1841]) << 32 | raw_data[1840])
      .SetData921(static_cast<int64_t>(raw_data[1843]) << 32 | raw_data[1842])
      .SetData922(static_cast<int64_t>(raw_data[1845]) << 32 | raw_data[1844])
      .SetData923(static_cast<int64_t>(raw_data[1847]) << 32 | raw_data[1846])
      .SetData924(static_cast<int64_t>(raw_data[1849]) << 32 | raw_data[1848])
      .SetData925(static_cast<int64_t>(raw_data[1851]) << 32 | raw_data[1850])
      .SetData926(static_cast<int64_t>(raw_data[1853]) << 32 | raw_data[1852])
      .SetData927(static_cast<int64_t>(raw_data[1855]) << 32 | raw_data[1854])
      .SetData928(static_cast<int64_t>(raw_data[1857]) << 32 | raw_data[1856])
      .SetData929(static_cast<int64_t>(raw_data[1859]) << 32 | raw_data[1858])
      .SetData930(static_cast<int64_t>(raw_data[1861]) << 32 | raw_data[1860])
      .SetData931(static_cast<int64_t>(raw_data[1863]) << 32 | raw_data[1862])
      .SetData932(static_cast<int64_t>(raw_data[1865]) << 32 | raw_data[1864])
      .SetData933(static_cast<int64_t>(raw_data[1867]) << 32 | raw_data[1866])
      .SetData934(static_cast<int64_t>(raw_data[1869]) << 32 | raw_data[1868])
      .SetData935(static_cast<int64_t>(raw_data[1871]) << 32 | raw_data[1870])
      .SetData936(static_cast<int64_t>(raw_data[1873]) << 32 | raw_data[1872])
      .SetData937(static_cast<int64_t>(raw_data[1875]) << 32 | raw_data[1874])
      .SetData938(static_cast<int64_t>(raw_data[1877]) << 32 | raw_data[1876])
      .SetData939(static_cast<int64_t>(raw_data[1879]) << 32 | raw_data[1878])
      .SetData940(static_cast<int64_t>(raw_data[1881]) << 32 | raw_data[1880])
      .SetData941(static_cast<int64_t>(raw_data[1883]) << 32 | raw_data[1882])
      .SetData942(static_cast<int64_t>(raw_data[1885]) << 32 | raw_data[1884])
      .SetData943(static_cast<int64_t>(raw_data[1887]) << 32 | raw_data[1886])
      .SetData944(static_cast<int64_t>(raw_data[1889]) << 32 | raw_data[1888])
      .SetData945(static_cast<int64_t>(raw_data[1891]) << 32 | raw_data[1890])
      .SetData946(static_cast<int64_t>(raw_data[1893]) << 32 | raw_data[1892])
      .SetData947(static_cast<int64_t>(raw_data[1895]) << 32 | raw_data[1894])
      .SetData948(static_cast<int64_t>(raw_data[1897]) << 32 | raw_data[1896])
      .SetData949(static_cast<int64_t>(raw_data[1899]) << 32 | raw_data[1898])
      .SetData950(static_cast<int64_t>(raw_data[1901]) << 32 | raw_data[1900])
      .SetData951(static_cast<int64_t>(raw_data[1903]) << 32 | raw_data[1902])
      .SetData952(static_cast<int64_t>(raw_data[1905]) << 32 | raw_data[1904])
      .SetData953(static_cast<int64_t>(raw_data[1907]) << 32 | raw_data[1906])
      .SetData954(static_cast<int64_t>(raw_data[1909]) << 32 | raw_data[1908])
      .SetData955(static_cast<int64_t>(raw_data[1911]) << 32 | raw_data[1910])
      .SetData956(static_cast<int64_t>(raw_data[1913]) << 32 | raw_data[1912])
      .SetData957(static_cast<int64_t>(raw_data[1915]) << 32 | raw_data[1914])
      .SetData958(static_cast<int64_t>(raw_data[1917]) << 32 | raw_data[1916])
      .SetData959(static_cast<int64_t>(raw_data[1919]) << 32 | raw_data[1918])
      .SetData960(static_cast<int64_t>(raw_data[1921]) << 32 | raw_data[1920])
      .SetData961(static_cast<int64_t>(raw_data[1923]) << 32 | raw_data[1922])
      .SetData962(static_cast<int64_t>(raw_data[1925]) << 32 | raw_data[1924])
      .SetData963(static_cast<int64_t>(raw_data[1927]) << 32 | raw_data[1926])
      .SetData964(static_cast<int64_t>(raw_data[1929]) << 32 | raw_data[1928])
      .SetData965(static_cast<int64_t>(raw_data[1931]) << 32 | raw_data[1930])
      .SetData966(static_cast<int64_t>(raw_data[1933]) << 32 | raw_data[1932])
      .SetData967(static_cast<int64_t>(raw_data[1935]) << 32 | raw_data[1934])
      .SetData968(static_cast<int64_t>(raw_data[1937]) << 32 | raw_data[1936])
      .SetData969(static_cast<int64_t>(raw_data[1939]) << 32 | raw_data[1938])
      .SetData970(static_cast<int64_t>(raw_data[1941]) << 32 | raw_data[1940])
      .SetData971(static_cast<int64_t>(raw_data[1943]) << 32 | raw_data[1942])
      .SetData972(static_cast<int64_t>(raw_data[1945]) << 32 | raw_data[1944])
      .SetData973(static_cast<int64_t>(raw_data[1947]) << 32 | raw_data[1946])
      .SetData974(static_cast<int64_t>(raw_data[1949]) << 32 | raw_data[1948])
      .SetData975(static_cast<int64_t>(raw_data[1951]) << 32 | raw_data[1950])
      .SetData976(static_cast<int64_t>(raw_data[1953]) << 32 | raw_data[1952])
      .SetData977(static_cast<int64_t>(raw_data[1955]) << 32 | raw_data[1954])
      .SetData978(static_cast<int64_t>(raw_data[1957]) << 32 | raw_data[1956])
      .SetData979(static_cast<int64_t>(raw_data[1959]) << 32 | raw_data[1958])
      .SetData980(static_cast<int64_t>(raw_data[1961]) << 32 | raw_data[1960])
      .SetData981(static_cast<int64_t>(raw_data[1963]) << 32 | raw_data[1962])
      .SetData982(static_cast<int64_t>(raw_data[1965]) << 32 | raw_data[1964])
      .SetData983(static_cast<int64_t>(raw_data[1967]) << 32 | raw_data[1966])
      .SetData984(static_cast<int64_t>(raw_data[1969]) << 32 | raw_data[1968])
      .SetData985(static_cast<int64_t>(raw_data[1971]) << 32 | raw_data[1970])
      .SetData986(static_cast<int64_t>(raw_data[1973]) << 32 | raw_data[1972])
      .SetData987(static_cast<int64_t>(raw_data[1975]) << 32 | raw_data[1974])
      .SetData988(static_cast<int64_t>(raw_data[1977]) << 32 | raw_data[1976])
      .SetData989(static_cast<int64_t>(raw_data[1979]) << 32 | raw_data[1978])
      .SetData990(static_cast<int64_t>(raw_data[1981]) << 32 | raw_data[1980])
      .SetData991(static_cast<int64_t>(raw_data[1983]) << 32 | raw_data[1982])
      .SetData992(static_cast<int64_t>(raw_data[1985]) << 32 | raw_data[1984])
      .SetData993(static_cast<int64_t>(raw_data[1987]) << 32 | raw_data[1986])
      .SetData994(static_cast<int64_t>(raw_data[1989]) << 32 | raw_data[1988])
      .SetData995(static_cast<int64_t>(raw_data[1991]) << 32 | raw_data[1990])
      .SetData996(static_cast<int64_t>(raw_data[1993]) << 32 | raw_data[1992])
      .SetData997(static_cast<int64_t>(raw_data[1995]) << 32 | raw_data[1994])
      .SetData998(static_cast<int64_t>(raw_data[1997]) << 32 | raw_data[1996])
      .SetData999(static_cast<int64_t>(raw_data[1999]) << 32 | raw_data[1998])
      .SetData1000(static_cast<int64_t>(raw_data[2001]) << 32 | raw_data[2000])
      .SetData1001(static_cast<int64_t>(raw_data[2003]) << 32 | raw_data[2002])
      .SetData1002(static_cast<int64_t>(raw_data[2005]) << 32 | raw_data[2004])
      .SetData1003(static_cast<int64_t>(raw_data[2007]) << 32 | raw_data[2006])
      .SetData1004(static_cast<int64_t>(raw_data[2009]) << 32 | raw_data[2008])
      .SetData1005(static_cast<int64_t>(raw_data[2011]) << 32 | raw_data[2010])
      .SetData1006(static_cast<int64_t>(raw_data[2013]) << 32 | raw_data[2012])
      .SetData1007(static_cast<int64_t>(raw_data[2015]) << 32 | raw_data[2014])
      .SetData1008(static_cast<int64_t>(raw_data[2017]) << 32 | raw_data[2016])
      .SetData1009(static_cast<int64_t>(raw_data[2019]) << 32 | raw_data[2018])
      .SetData1010(static_cast<int64_t>(raw_data[2021]) << 32 | raw_data[2020])
      .SetData1011(static_cast<int64_t>(raw_data[2023]) << 32 | raw_data[2022])
      .SetData1012(static_cast<int64_t>(raw_data[2025]) << 32 | raw_data[2024])
      .SetData1013(static_cast<int64_t>(raw_data[2027]) << 32 | raw_data[2026])
      .SetData1014(static_cast<int64_t>(raw_data[2029]) << 32 | raw_data[2028])
      .SetData1015(static_cast<int64_t>(raw_data[2031]) << 32 | raw_data[2030])
      .SetData1016(static_cast<int64_t>(raw_data[2033]) << 32 | raw_data[2032])
      .SetData1017(static_cast<int64_t>(raw_data[2035]) << 32 | raw_data[2034])
      .SetData1018(static_cast<int64_t>(raw_data[2037]) << 32 | raw_data[2036])
      .SetData1019(static_cast<int64_t>(raw_data[2039]) << 32 | raw_data[2038])
      .SetData1020(static_cast<int64_t>(raw_data[2041]) << 32 | raw_data[2040])
      .SetData1021(static_cast<int64_t>(raw_data[2043]) << 32 | raw_data[2042])
      .SetData1022(static_cast<int64_t>(raw_data[2045]) << 32 | raw_data[2044])
      .SetData1023(static_cast<int64_t>(raw_data[2047]) << 32 | raw_data[2046])
      .Record(ukm_recorder);
  return true;
}

void V8CrowdsourcedCompileHintsProducer::AddNoise(unsigned* data) {
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

}  // namespace blink::v8_compile_hints

#endif  // BUILDFLAG(PRODUCE_V8_COMPILE_HINTS)
