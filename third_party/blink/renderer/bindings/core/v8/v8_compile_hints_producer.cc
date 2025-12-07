// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

constexpr double kProduceCompileHintsNoiseLevel = 0.5;
#if BUILDFLAG(IS_WIN)
constexpr float kProduceCompileHintsDataProductionLevel = 0.005;
#endif  // BUILDFLAG(IS_WIN)

bool RandomlySelectedToGenerateData() {
  // Data collection is only enabled on Windows. TODO(chromium:1406506): enable
  // on more platforms.
#if BUILDFLAG(IS_WIN)
  // Decide whether we collect the data based on client-side randomization.
  // This is further subject to UKM restrictions: whether the user has enabled
  // the data collection + downsampling. See crbug.com/1483975.
  return base::RandDouble() < kProduceCompileHintsDataProductionLevel;
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

// static
bool& V8CrowdsourcedCompileHintsProducer::DisableCompileHintsForTesting() {
  static bool disable = false;
  return disable;
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

  execution_context->GetTaskRunner(TaskType::kInternalDefault)
      ->PostDelayedTask(
          FROM_HERE, BindOnce(&ClearDataTask, WrapWeakPersistent(this)), delay);
}

bool V8CrowdsourcedCompileHintsProducer::MightGenerateData() {
  // Force disable compile hints generation for testing.
  if (DisableCompileHintsForTesting()) {
    return false;
  }

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
  BloomFilter<kBloomFilterKeySize> bloom;

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
  auto raw_data = bloom.GetRawData();

  // Add noise to the data.
  for (int i = 0; i < kBloomFilterInt32Count; ++i) {
    AddNoise(&raw_data[i]);
  }

  // Send the data to UKM.
  DCHECK_NE(execution_context->UkmSourceID(), ukm::kInvalidSourceId);
  ukm::UkmRecorder* ukm_recorder = execution_context->UkmRecorder();

  ukm::builders::V8CompileHints_Version5 ch(execution_context->UkmSourceID());
  ch.SetData0(static_cast<int64_t>(raw_data[1]) << 32 | raw_data[0]);
  ch.SetData1(static_cast<int64_t>(raw_data[3]) << 32 | raw_data[2]);
  ch.SetData2(static_cast<int64_t>(raw_data[5]) << 32 | raw_data[4]);
  ch.SetData3(static_cast<int64_t>(raw_data[7]) << 32 | raw_data[6]);
  ch.SetData4(static_cast<int64_t>(raw_data[9]) << 32 | raw_data[8]);
  ch.SetData5(static_cast<int64_t>(raw_data[11]) << 32 | raw_data[10]);
  ch.SetData6(static_cast<int64_t>(raw_data[13]) << 32 | raw_data[12]);
  ch.SetData7(static_cast<int64_t>(raw_data[15]) << 32 | raw_data[14]);
  ch.SetData8(static_cast<int64_t>(raw_data[17]) << 32 | raw_data[16]);
  ch.SetData9(static_cast<int64_t>(raw_data[19]) << 32 | raw_data[18]);
  ch.SetData10(static_cast<int64_t>(raw_data[21]) << 32 | raw_data[20]);
  ch.SetData11(static_cast<int64_t>(raw_data[23]) << 32 | raw_data[22]);
  ch.SetData12(static_cast<int64_t>(raw_data[25]) << 32 | raw_data[24]);
  ch.SetData13(static_cast<int64_t>(raw_data[27]) << 32 | raw_data[26]);
  ch.SetData14(static_cast<int64_t>(raw_data[29]) << 32 | raw_data[28]);
  ch.SetData15(static_cast<int64_t>(raw_data[31]) << 32 | raw_data[30]);
  ch.SetData16(static_cast<int64_t>(raw_data[33]) << 32 | raw_data[32]);
  ch.SetData17(static_cast<int64_t>(raw_data[35]) << 32 | raw_data[34]);
  ch.SetData18(static_cast<int64_t>(raw_data[37]) << 32 | raw_data[36]);
  ch.SetData19(static_cast<int64_t>(raw_data[39]) << 32 | raw_data[38]);
  ch.SetData20(static_cast<int64_t>(raw_data[41]) << 32 | raw_data[40]);
  ch.SetData21(static_cast<int64_t>(raw_data[43]) << 32 | raw_data[42]);
  ch.SetData22(static_cast<int64_t>(raw_data[45]) << 32 | raw_data[44]);
  ch.SetData23(static_cast<int64_t>(raw_data[47]) << 32 | raw_data[46]);
  ch.SetData24(static_cast<int64_t>(raw_data[49]) << 32 | raw_data[48]);
  ch.SetData25(static_cast<int64_t>(raw_data[51]) << 32 | raw_data[50]);
  ch.SetData26(static_cast<int64_t>(raw_data[53]) << 32 | raw_data[52]);
  ch.SetData27(static_cast<int64_t>(raw_data[55]) << 32 | raw_data[54]);
  ch.SetData28(static_cast<int64_t>(raw_data[57]) << 32 | raw_data[56]);
  ch.SetData29(static_cast<int64_t>(raw_data[59]) << 32 | raw_data[58]);
  ch.SetData30(static_cast<int64_t>(raw_data[61]) << 32 | raw_data[60]);
  ch.SetData31(static_cast<int64_t>(raw_data[63]) << 32 | raw_data[62]);
  ch.SetData32(static_cast<int64_t>(raw_data[65]) << 32 | raw_data[64]);
  ch.SetData33(static_cast<int64_t>(raw_data[67]) << 32 | raw_data[66]);
  ch.SetData34(static_cast<int64_t>(raw_data[69]) << 32 | raw_data[68]);
  ch.SetData35(static_cast<int64_t>(raw_data[71]) << 32 | raw_data[70]);
  ch.SetData36(static_cast<int64_t>(raw_data[73]) << 32 | raw_data[72]);
  ch.SetData37(static_cast<int64_t>(raw_data[75]) << 32 | raw_data[74]);
  ch.SetData38(static_cast<int64_t>(raw_data[77]) << 32 | raw_data[76]);
  ch.SetData39(static_cast<int64_t>(raw_data[79]) << 32 | raw_data[78]);
  ch.SetData40(static_cast<int64_t>(raw_data[81]) << 32 | raw_data[80]);
  ch.SetData41(static_cast<int64_t>(raw_data[83]) << 32 | raw_data[82]);
  ch.SetData42(static_cast<int64_t>(raw_data[85]) << 32 | raw_data[84]);
  ch.SetData43(static_cast<int64_t>(raw_data[87]) << 32 | raw_data[86]);
  ch.SetData44(static_cast<int64_t>(raw_data[89]) << 32 | raw_data[88]);
  ch.SetData45(static_cast<int64_t>(raw_data[91]) << 32 | raw_data[90]);
  ch.SetData46(static_cast<int64_t>(raw_data[93]) << 32 | raw_data[92]);
  ch.SetData47(static_cast<int64_t>(raw_data[95]) << 32 | raw_data[94]);
  ch.SetData48(static_cast<int64_t>(raw_data[97]) << 32 | raw_data[96]);
  ch.SetData49(static_cast<int64_t>(raw_data[99]) << 32 | raw_data[98]);
  ch.SetData50(static_cast<int64_t>(raw_data[101]) << 32 | raw_data[100]);
  ch.SetData51(static_cast<int64_t>(raw_data[103]) << 32 | raw_data[102]);
  ch.SetData52(static_cast<int64_t>(raw_data[105]) << 32 | raw_data[104]);
  ch.SetData53(static_cast<int64_t>(raw_data[107]) << 32 | raw_data[106]);
  ch.SetData54(static_cast<int64_t>(raw_data[109]) << 32 | raw_data[108]);
  ch.SetData55(static_cast<int64_t>(raw_data[111]) << 32 | raw_data[110]);
  ch.SetData56(static_cast<int64_t>(raw_data[113]) << 32 | raw_data[112]);
  ch.SetData57(static_cast<int64_t>(raw_data[115]) << 32 | raw_data[114]);
  ch.SetData58(static_cast<int64_t>(raw_data[117]) << 32 | raw_data[116]);
  ch.SetData59(static_cast<int64_t>(raw_data[119]) << 32 | raw_data[118]);
  ch.SetData60(static_cast<int64_t>(raw_data[121]) << 32 | raw_data[120]);
  ch.SetData61(static_cast<int64_t>(raw_data[123]) << 32 | raw_data[122]);
  ch.SetData62(static_cast<int64_t>(raw_data[125]) << 32 | raw_data[124]);
  ch.SetData63(static_cast<int64_t>(raw_data[127]) << 32 | raw_data[126]);
  ch.SetData64(static_cast<int64_t>(raw_data[129]) << 32 | raw_data[128]);
  ch.SetData65(static_cast<int64_t>(raw_data[131]) << 32 | raw_data[130]);
  ch.SetData66(static_cast<int64_t>(raw_data[133]) << 32 | raw_data[132]);
  ch.SetData67(static_cast<int64_t>(raw_data[135]) << 32 | raw_data[134]);
  ch.SetData68(static_cast<int64_t>(raw_data[137]) << 32 | raw_data[136]);
  ch.SetData69(static_cast<int64_t>(raw_data[139]) << 32 | raw_data[138]);
  ch.SetData70(static_cast<int64_t>(raw_data[141]) << 32 | raw_data[140]);
  ch.SetData71(static_cast<int64_t>(raw_data[143]) << 32 | raw_data[142]);
  ch.SetData72(static_cast<int64_t>(raw_data[145]) << 32 | raw_data[144]);
  ch.SetData73(static_cast<int64_t>(raw_data[147]) << 32 | raw_data[146]);
  ch.SetData74(static_cast<int64_t>(raw_data[149]) << 32 | raw_data[148]);
  ch.SetData75(static_cast<int64_t>(raw_data[151]) << 32 | raw_data[150]);
  ch.SetData76(static_cast<int64_t>(raw_data[153]) << 32 | raw_data[152]);
  ch.SetData77(static_cast<int64_t>(raw_data[155]) << 32 | raw_data[154]);
  ch.SetData78(static_cast<int64_t>(raw_data[157]) << 32 | raw_data[156]);
  ch.SetData79(static_cast<int64_t>(raw_data[159]) << 32 | raw_data[158]);
  ch.SetData80(static_cast<int64_t>(raw_data[161]) << 32 | raw_data[160]);
  ch.SetData81(static_cast<int64_t>(raw_data[163]) << 32 | raw_data[162]);
  ch.SetData82(static_cast<int64_t>(raw_data[165]) << 32 | raw_data[164]);
  ch.SetData83(static_cast<int64_t>(raw_data[167]) << 32 | raw_data[166]);
  ch.SetData84(static_cast<int64_t>(raw_data[169]) << 32 | raw_data[168]);
  ch.SetData85(static_cast<int64_t>(raw_data[171]) << 32 | raw_data[170]);
  ch.SetData86(static_cast<int64_t>(raw_data[173]) << 32 | raw_data[172]);
  ch.SetData87(static_cast<int64_t>(raw_data[175]) << 32 | raw_data[174]);
  ch.SetData88(static_cast<int64_t>(raw_data[177]) << 32 | raw_data[176]);
  ch.SetData89(static_cast<int64_t>(raw_data[179]) << 32 | raw_data[178]);
  ch.SetData90(static_cast<int64_t>(raw_data[181]) << 32 | raw_data[180]);
  ch.SetData91(static_cast<int64_t>(raw_data[183]) << 32 | raw_data[182]);
  ch.SetData92(static_cast<int64_t>(raw_data[185]) << 32 | raw_data[184]);
  ch.SetData93(static_cast<int64_t>(raw_data[187]) << 32 | raw_data[186]);
  ch.SetData94(static_cast<int64_t>(raw_data[189]) << 32 | raw_data[188]);
  ch.SetData95(static_cast<int64_t>(raw_data[191]) << 32 | raw_data[190]);
  ch.SetData96(static_cast<int64_t>(raw_data[193]) << 32 | raw_data[192]);
  ch.SetData97(static_cast<int64_t>(raw_data[195]) << 32 | raw_data[194]);
  ch.SetData98(static_cast<int64_t>(raw_data[197]) << 32 | raw_data[196]);
  ch.SetData99(static_cast<int64_t>(raw_data[199]) << 32 | raw_data[198]);
  ch.SetData100(static_cast<int64_t>(raw_data[201]) << 32 | raw_data[200]);
  ch.SetData101(static_cast<int64_t>(raw_data[203]) << 32 | raw_data[202]);
  ch.SetData102(static_cast<int64_t>(raw_data[205]) << 32 | raw_data[204]);
  ch.SetData103(static_cast<int64_t>(raw_data[207]) << 32 | raw_data[206]);
  ch.SetData104(static_cast<int64_t>(raw_data[209]) << 32 | raw_data[208]);
  ch.SetData105(static_cast<int64_t>(raw_data[211]) << 32 | raw_data[210]);
  ch.SetData106(static_cast<int64_t>(raw_data[213]) << 32 | raw_data[212]);
  ch.SetData107(static_cast<int64_t>(raw_data[215]) << 32 | raw_data[214]);
  ch.SetData108(static_cast<int64_t>(raw_data[217]) << 32 | raw_data[216]);
  ch.SetData109(static_cast<int64_t>(raw_data[219]) << 32 | raw_data[218]);
  ch.SetData110(static_cast<int64_t>(raw_data[221]) << 32 | raw_data[220]);
  ch.SetData111(static_cast<int64_t>(raw_data[223]) << 32 | raw_data[222]);
  ch.SetData112(static_cast<int64_t>(raw_data[225]) << 32 | raw_data[224]);
  ch.SetData113(static_cast<int64_t>(raw_data[227]) << 32 | raw_data[226]);
  ch.SetData114(static_cast<int64_t>(raw_data[229]) << 32 | raw_data[228]);
  ch.SetData115(static_cast<int64_t>(raw_data[231]) << 32 | raw_data[230]);
  ch.SetData116(static_cast<int64_t>(raw_data[233]) << 32 | raw_data[232]);
  ch.SetData117(static_cast<int64_t>(raw_data[235]) << 32 | raw_data[234]);
  ch.SetData118(static_cast<int64_t>(raw_data[237]) << 32 | raw_data[236]);
  ch.SetData119(static_cast<int64_t>(raw_data[239]) << 32 | raw_data[238]);
  ch.SetData120(static_cast<int64_t>(raw_data[241]) << 32 | raw_data[240]);
  ch.SetData121(static_cast<int64_t>(raw_data[243]) << 32 | raw_data[242]);
  ch.SetData122(static_cast<int64_t>(raw_data[245]) << 32 | raw_data[244]);
  ch.SetData123(static_cast<int64_t>(raw_data[247]) << 32 | raw_data[246]);
  ch.SetData124(static_cast<int64_t>(raw_data[249]) << 32 | raw_data[248]);
  ch.SetData125(static_cast<int64_t>(raw_data[251]) << 32 | raw_data[250]);
  ch.SetData126(static_cast<int64_t>(raw_data[253]) << 32 | raw_data[252]);
  ch.SetData127(static_cast<int64_t>(raw_data[255]) << 32 | raw_data[254]);
  ch.SetData128(static_cast<int64_t>(raw_data[257]) << 32 | raw_data[256]);
  ch.SetData129(static_cast<int64_t>(raw_data[259]) << 32 | raw_data[258]);
  ch.SetData130(static_cast<int64_t>(raw_data[261]) << 32 | raw_data[260]);
  ch.SetData131(static_cast<int64_t>(raw_data[263]) << 32 | raw_data[262]);
  ch.SetData132(static_cast<int64_t>(raw_data[265]) << 32 | raw_data[264]);
  ch.SetData133(static_cast<int64_t>(raw_data[267]) << 32 | raw_data[266]);
  ch.SetData134(static_cast<int64_t>(raw_data[269]) << 32 | raw_data[268]);
  ch.SetData135(static_cast<int64_t>(raw_data[271]) << 32 | raw_data[270]);
  ch.SetData136(static_cast<int64_t>(raw_data[273]) << 32 | raw_data[272]);
  ch.SetData137(static_cast<int64_t>(raw_data[275]) << 32 | raw_data[274]);
  ch.SetData138(static_cast<int64_t>(raw_data[277]) << 32 | raw_data[276]);
  ch.SetData139(static_cast<int64_t>(raw_data[279]) << 32 | raw_data[278]);
  ch.SetData140(static_cast<int64_t>(raw_data[281]) << 32 | raw_data[280]);
  ch.SetData141(static_cast<int64_t>(raw_data[283]) << 32 | raw_data[282]);
  ch.SetData142(static_cast<int64_t>(raw_data[285]) << 32 | raw_data[284]);
  ch.SetData143(static_cast<int64_t>(raw_data[287]) << 32 | raw_data[286]);
  ch.SetData144(static_cast<int64_t>(raw_data[289]) << 32 | raw_data[288]);
  ch.SetData145(static_cast<int64_t>(raw_data[291]) << 32 | raw_data[290]);
  ch.SetData146(static_cast<int64_t>(raw_data[293]) << 32 | raw_data[292]);
  ch.SetData147(static_cast<int64_t>(raw_data[295]) << 32 | raw_data[294]);
  ch.SetData148(static_cast<int64_t>(raw_data[297]) << 32 | raw_data[296]);
  ch.SetData149(static_cast<int64_t>(raw_data[299]) << 32 | raw_data[298]);
  ch.SetData150(static_cast<int64_t>(raw_data[301]) << 32 | raw_data[300]);
  ch.SetData151(static_cast<int64_t>(raw_data[303]) << 32 | raw_data[302]);
  ch.SetData152(static_cast<int64_t>(raw_data[305]) << 32 | raw_data[304]);
  ch.SetData153(static_cast<int64_t>(raw_data[307]) << 32 | raw_data[306]);
  ch.SetData154(static_cast<int64_t>(raw_data[309]) << 32 | raw_data[308]);
  ch.SetData155(static_cast<int64_t>(raw_data[311]) << 32 | raw_data[310]);
  ch.SetData156(static_cast<int64_t>(raw_data[313]) << 32 | raw_data[312]);
  ch.SetData157(static_cast<int64_t>(raw_data[315]) << 32 | raw_data[314]);
  ch.SetData158(static_cast<int64_t>(raw_data[317]) << 32 | raw_data[316]);
  ch.SetData159(static_cast<int64_t>(raw_data[319]) << 32 | raw_data[318]);
  ch.SetData160(static_cast<int64_t>(raw_data[321]) << 32 | raw_data[320]);
  ch.SetData161(static_cast<int64_t>(raw_data[323]) << 32 | raw_data[322]);
  ch.SetData162(static_cast<int64_t>(raw_data[325]) << 32 | raw_data[324]);
  ch.SetData163(static_cast<int64_t>(raw_data[327]) << 32 | raw_data[326]);
  ch.SetData164(static_cast<int64_t>(raw_data[329]) << 32 | raw_data[328]);
  ch.SetData165(static_cast<int64_t>(raw_data[331]) << 32 | raw_data[330]);
  ch.SetData166(static_cast<int64_t>(raw_data[333]) << 32 | raw_data[332]);
  ch.SetData167(static_cast<int64_t>(raw_data[335]) << 32 | raw_data[334]);
  ch.SetData168(static_cast<int64_t>(raw_data[337]) << 32 | raw_data[336]);
  ch.SetData169(static_cast<int64_t>(raw_data[339]) << 32 | raw_data[338]);
  ch.SetData170(static_cast<int64_t>(raw_data[341]) << 32 | raw_data[340]);
  ch.SetData171(static_cast<int64_t>(raw_data[343]) << 32 | raw_data[342]);
  ch.SetData172(static_cast<int64_t>(raw_data[345]) << 32 | raw_data[344]);
  ch.SetData173(static_cast<int64_t>(raw_data[347]) << 32 | raw_data[346]);
  ch.SetData174(static_cast<int64_t>(raw_data[349]) << 32 | raw_data[348]);
  ch.SetData175(static_cast<int64_t>(raw_data[351]) << 32 | raw_data[350]);
  ch.SetData176(static_cast<int64_t>(raw_data[353]) << 32 | raw_data[352]);
  ch.SetData177(static_cast<int64_t>(raw_data[355]) << 32 | raw_data[354]);
  ch.SetData178(static_cast<int64_t>(raw_data[357]) << 32 | raw_data[356]);
  ch.SetData179(static_cast<int64_t>(raw_data[359]) << 32 | raw_data[358]);
  ch.SetData180(static_cast<int64_t>(raw_data[361]) << 32 | raw_data[360]);
  ch.SetData181(static_cast<int64_t>(raw_data[363]) << 32 | raw_data[362]);
  ch.SetData182(static_cast<int64_t>(raw_data[365]) << 32 | raw_data[364]);
  ch.SetData183(static_cast<int64_t>(raw_data[367]) << 32 | raw_data[366]);
  ch.SetData184(static_cast<int64_t>(raw_data[369]) << 32 | raw_data[368]);
  ch.SetData185(static_cast<int64_t>(raw_data[371]) << 32 | raw_data[370]);
  ch.SetData186(static_cast<int64_t>(raw_data[373]) << 32 | raw_data[372]);
  ch.SetData187(static_cast<int64_t>(raw_data[375]) << 32 | raw_data[374]);
  ch.SetData188(static_cast<int64_t>(raw_data[377]) << 32 | raw_data[376]);
  ch.SetData189(static_cast<int64_t>(raw_data[379]) << 32 | raw_data[378]);
  ch.SetData190(static_cast<int64_t>(raw_data[381]) << 32 | raw_data[380]);
  ch.SetData191(static_cast<int64_t>(raw_data[383]) << 32 | raw_data[382]);
  ch.SetData192(static_cast<int64_t>(raw_data[385]) << 32 | raw_data[384]);
  ch.SetData193(static_cast<int64_t>(raw_data[387]) << 32 | raw_data[386]);
  ch.SetData194(static_cast<int64_t>(raw_data[389]) << 32 | raw_data[388]);
  ch.SetData195(static_cast<int64_t>(raw_data[391]) << 32 | raw_data[390]);
  ch.SetData196(static_cast<int64_t>(raw_data[393]) << 32 | raw_data[392]);
  ch.SetData197(static_cast<int64_t>(raw_data[395]) << 32 | raw_data[394]);
  ch.SetData198(static_cast<int64_t>(raw_data[397]) << 32 | raw_data[396]);
  ch.SetData199(static_cast<int64_t>(raw_data[399]) << 32 | raw_data[398]);
  ch.SetData200(static_cast<int64_t>(raw_data[401]) << 32 | raw_data[400]);
  ch.SetData201(static_cast<int64_t>(raw_data[403]) << 32 | raw_data[402]);
  ch.SetData202(static_cast<int64_t>(raw_data[405]) << 32 | raw_data[404]);
  ch.SetData203(static_cast<int64_t>(raw_data[407]) << 32 | raw_data[406]);
  ch.SetData204(static_cast<int64_t>(raw_data[409]) << 32 | raw_data[408]);
  ch.SetData205(static_cast<int64_t>(raw_data[411]) << 32 | raw_data[410]);
  ch.SetData206(static_cast<int64_t>(raw_data[413]) << 32 | raw_data[412]);
  ch.SetData207(static_cast<int64_t>(raw_data[415]) << 32 | raw_data[414]);
  ch.SetData208(static_cast<int64_t>(raw_data[417]) << 32 | raw_data[416]);
  ch.SetData209(static_cast<int64_t>(raw_data[419]) << 32 | raw_data[418]);
  ch.SetData210(static_cast<int64_t>(raw_data[421]) << 32 | raw_data[420]);
  ch.SetData211(static_cast<int64_t>(raw_data[423]) << 32 | raw_data[422]);
  ch.SetData212(static_cast<int64_t>(raw_data[425]) << 32 | raw_data[424]);
  ch.SetData213(static_cast<int64_t>(raw_data[427]) << 32 | raw_data[426]);
  ch.SetData214(static_cast<int64_t>(raw_data[429]) << 32 | raw_data[428]);
  ch.SetData215(static_cast<int64_t>(raw_data[431]) << 32 | raw_data[430]);
  ch.SetData216(static_cast<int64_t>(raw_data[433]) << 32 | raw_data[432]);
  ch.SetData217(static_cast<int64_t>(raw_data[435]) << 32 | raw_data[434]);
  ch.SetData218(static_cast<int64_t>(raw_data[437]) << 32 | raw_data[436]);
  ch.SetData219(static_cast<int64_t>(raw_data[439]) << 32 | raw_data[438]);
  ch.SetData220(static_cast<int64_t>(raw_data[441]) << 32 | raw_data[440]);
  ch.SetData221(static_cast<int64_t>(raw_data[443]) << 32 | raw_data[442]);
  ch.SetData222(static_cast<int64_t>(raw_data[445]) << 32 | raw_data[444]);
  ch.SetData223(static_cast<int64_t>(raw_data[447]) << 32 | raw_data[446]);
  ch.SetData224(static_cast<int64_t>(raw_data[449]) << 32 | raw_data[448]);
  ch.SetData225(static_cast<int64_t>(raw_data[451]) << 32 | raw_data[450]);
  ch.SetData226(static_cast<int64_t>(raw_data[453]) << 32 | raw_data[452]);
  ch.SetData227(static_cast<int64_t>(raw_data[455]) << 32 | raw_data[454]);
  ch.SetData228(static_cast<int64_t>(raw_data[457]) << 32 | raw_data[456]);
  ch.SetData229(static_cast<int64_t>(raw_data[459]) << 32 | raw_data[458]);
  ch.SetData230(static_cast<int64_t>(raw_data[461]) << 32 | raw_data[460]);
  ch.SetData231(static_cast<int64_t>(raw_data[463]) << 32 | raw_data[462]);
  ch.SetData232(static_cast<int64_t>(raw_data[465]) << 32 | raw_data[464]);
  ch.SetData233(static_cast<int64_t>(raw_data[467]) << 32 | raw_data[466]);
  ch.SetData234(static_cast<int64_t>(raw_data[469]) << 32 | raw_data[468]);
  ch.SetData235(static_cast<int64_t>(raw_data[471]) << 32 | raw_data[470]);
  ch.SetData236(static_cast<int64_t>(raw_data[473]) << 32 | raw_data[472]);
  ch.SetData237(static_cast<int64_t>(raw_data[475]) << 32 | raw_data[474]);
  ch.SetData238(static_cast<int64_t>(raw_data[477]) << 32 | raw_data[476]);
  ch.SetData239(static_cast<int64_t>(raw_data[479]) << 32 | raw_data[478]);
  ch.SetData240(static_cast<int64_t>(raw_data[481]) << 32 | raw_data[480]);
  ch.SetData241(static_cast<int64_t>(raw_data[483]) << 32 | raw_data[482]);
  ch.SetData242(static_cast<int64_t>(raw_data[485]) << 32 | raw_data[484]);
  ch.SetData243(static_cast<int64_t>(raw_data[487]) << 32 | raw_data[486]);
  ch.SetData244(static_cast<int64_t>(raw_data[489]) << 32 | raw_data[488]);
  ch.SetData245(static_cast<int64_t>(raw_data[491]) << 32 | raw_data[490]);
  ch.SetData246(static_cast<int64_t>(raw_data[493]) << 32 | raw_data[492]);
  ch.SetData247(static_cast<int64_t>(raw_data[495]) << 32 | raw_data[494]);
  ch.SetData248(static_cast<int64_t>(raw_data[497]) << 32 | raw_data[496]);
  ch.SetData249(static_cast<int64_t>(raw_data[499]) << 32 | raw_data[498]);
  ch.SetData250(static_cast<int64_t>(raw_data[501]) << 32 | raw_data[500]);
  ch.SetData251(static_cast<int64_t>(raw_data[503]) << 32 | raw_data[502]);
  ch.SetData252(static_cast<int64_t>(raw_data[505]) << 32 | raw_data[504]);
  ch.SetData253(static_cast<int64_t>(raw_data[507]) << 32 | raw_data[506]);
  ch.SetData254(static_cast<int64_t>(raw_data[509]) << 32 | raw_data[508]);
  ch.SetData255(static_cast<int64_t>(raw_data[511]) << 32 | raw_data[510]);
  ch.SetData256(static_cast<int64_t>(raw_data[513]) << 32 | raw_data[512]);
  ch.SetData257(static_cast<int64_t>(raw_data[515]) << 32 | raw_data[514]);
  ch.SetData258(static_cast<int64_t>(raw_data[517]) << 32 | raw_data[516]);
  ch.SetData259(static_cast<int64_t>(raw_data[519]) << 32 | raw_data[518]);
  ch.SetData260(static_cast<int64_t>(raw_data[521]) << 32 | raw_data[520]);
  ch.SetData261(static_cast<int64_t>(raw_data[523]) << 32 | raw_data[522]);
  ch.SetData262(static_cast<int64_t>(raw_data[525]) << 32 | raw_data[524]);
  ch.SetData263(static_cast<int64_t>(raw_data[527]) << 32 | raw_data[526]);
  ch.SetData264(static_cast<int64_t>(raw_data[529]) << 32 | raw_data[528]);
  ch.SetData265(static_cast<int64_t>(raw_data[531]) << 32 | raw_data[530]);
  ch.SetData266(static_cast<int64_t>(raw_data[533]) << 32 | raw_data[532]);
  ch.SetData267(static_cast<int64_t>(raw_data[535]) << 32 | raw_data[534]);
  ch.SetData268(static_cast<int64_t>(raw_data[537]) << 32 | raw_data[536]);
  ch.SetData269(static_cast<int64_t>(raw_data[539]) << 32 | raw_data[538]);
  ch.SetData270(static_cast<int64_t>(raw_data[541]) << 32 | raw_data[540]);
  ch.SetData271(static_cast<int64_t>(raw_data[543]) << 32 | raw_data[542]);
  ch.SetData272(static_cast<int64_t>(raw_data[545]) << 32 | raw_data[544]);
  ch.SetData273(static_cast<int64_t>(raw_data[547]) << 32 | raw_data[546]);
  ch.SetData274(static_cast<int64_t>(raw_data[549]) << 32 | raw_data[548]);
  ch.SetData275(static_cast<int64_t>(raw_data[551]) << 32 | raw_data[550]);
  ch.SetData276(static_cast<int64_t>(raw_data[553]) << 32 | raw_data[552]);
  ch.SetData277(static_cast<int64_t>(raw_data[555]) << 32 | raw_data[554]);
  ch.SetData278(static_cast<int64_t>(raw_data[557]) << 32 | raw_data[556]);
  ch.SetData279(static_cast<int64_t>(raw_data[559]) << 32 | raw_data[558]);
  ch.SetData280(static_cast<int64_t>(raw_data[561]) << 32 | raw_data[560]);
  ch.SetData281(static_cast<int64_t>(raw_data[563]) << 32 | raw_data[562]);
  ch.SetData282(static_cast<int64_t>(raw_data[565]) << 32 | raw_data[564]);
  ch.SetData283(static_cast<int64_t>(raw_data[567]) << 32 | raw_data[566]);
  ch.SetData284(static_cast<int64_t>(raw_data[569]) << 32 | raw_data[568]);
  ch.SetData285(static_cast<int64_t>(raw_data[571]) << 32 | raw_data[570]);
  ch.SetData286(static_cast<int64_t>(raw_data[573]) << 32 | raw_data[572]);
  ch.SetData287(static_cast<int64_t>(raw_data[575]) << 32 | raw_data[574]);
  ch.SetData288(static_cast<int64_t>(raw_data[577]) << 32 | raw_data[576]);
  ch.SetData289(static_cast<int64_t>(raw_data[579]) << 32 | raw_data[578]);
  ch.SetData290(static_cast<int64_t>(raw_data[581]) << 32 | raw_data[580]);
  ch.SetData291(static_cast<int64_t>(raw_data[583]) << 32 | raw_data[582]);
  ch.SetData292(static_cast<int64_t>(raw_data[585]) << 32 | raw_data[584]);
  ch.SetData293(static_cast<int64_t>(raw_data[587]) << 32 | raw_data[586]);
  ch.SetData294(static_cast<int64_t>(raw_data[589]) << 32 | raw_data[588]);
  ch.SetData295(static_cast<int64_t>(raw_data[591]) << 32 | raw_data[590]);
  ch.SetData296(static_cast<int64_t>(raw_data[593]) << 32 | raw_data[592]);
  ch.SetData297(static_cast<int64_t>(raw_data[595]) << 32 | raw_data[594]);
  ch.SetData298(static_cast<int64_t>(raw_data[597]) << 32 | raw_data[596]);
  ch.SetData299(static_cast<int64_t>(raw_data[599]) << 32 | raw_data[598]);
  ch.SetData300(static_cast<int64_t>(raw_data[601]) << 32 | raw_data[600]);
  ch.SetData301(static_cast<int64_t>(raw_data[603]) << 32 | raw_data[602]);
  ch.SetData302(static_cast<int64_t>(raw_data[605]) << 32 | raw_data[604]);
  ch.SetData303(static_cast<int64_t>(raw_data[607]) << 32 | raw_data[606]);
  ch.SetData304(static_cast<int64_t>(raw_data[609]) << 32 | raw_data[608]);
  ch.SetData305(static_cast<int64_t>(raw_data[611]) << 32 | raw_data[610]);
  ch.SetData306(static_cast<int64_t>(raw_data[613]) << 32 | raw_data[612]);
  ch.SetData307(static_cast<int64_t>(raw_data[615]) << 32 | raw_data[614]);
  ch.SetData308(static_cast<int64_t>(raw_data[617]) << 32 | raw_data[616]);
  ch.SetData309(static_cast<int64_t>(raw_data[619]) << 32 | raw_data[618]);
  ch.SetData310(static_cast<int64_t>(raw_data[621]) << 32 | raw_data[620]);
  ch.SetData311(static_cast<int64_t>(raw_data[623]) << 32 | raw_data[622]);
  ch.SetData312(static_cast<int64_t>(raw_data[625]) << 32 | raw_data[624]);
  ch.SetData313(static_cast<int64_t>(raw_data[627]) << 32 | raw_data[626]);
  ch.SetData314(static_cast<int64_t>(raw_data[629]) << 32 | raw_data[628]);
  ch.SetData315(static_cast<int64_t>(raw_data[631]) << 32 | raw_data[630]);
  ch.SetData316(static_cast<int64_t>(raw_data[633]) << 32 | raw_data[632]);
  ch.SetData317(static_cast<int64_t>(raw_data[635]) << 32 | raw_data[634]);
  ch.SetData318(static_cast<int64_t>(raw_data[637]) << 32 | raw_data[636]);
  ch.SetData319(static_cast<int64_t>(raw_data[639]) << 32 | raw_data[638]);
  ch.SetData320(static_cast<int64_t>(raw_data[641]) << 32 | raw_data[640]);
  ch.SetData321(static_cast<int64_t>(raw_data[643]) << 32 | raw_data[642]);
  ch.SetData322(static_cast<int64_t>(raw_data[645]) << 32 | raw_data[644]);
  ch.SetData323(static_cast<int64_t>(raw_data[647]) << 32 | raw_data[646]);
  ch.SetData324(static_cast<int64_t>(raw_data[649]) << 32 | raw_data[648]);
  ch.SetData325(static_cast<int64_t>(raw_data[651]) << 32 | raw_data[650]);
  ch.SetData326(static_cast<int64_t>(raw_data[653]) << 32 | raw_data[652]);
  ch.SetData327(static_cast<int64_t>(raw_data[655]) << 32 | raw_data[654]);
  ch.SetData328(static_cast<int64_t>(raw_data[657]) << 32 | raw_data[656]);
  ch.SetData329(static_cast<int64_t>(raw_data[659]) << 32 | raw_data[658]);
  ch.SetData330(static_cast<int64_t>(raw_data[661]) << 32 | raw_data[660]);
  ch.SetData331(static_cast<int64_t>(raw_data[663]) << 32 | raw_data[662]);
  ch.SetData332(static_cast<int64_t>(raw_data[665]) << 32 | raw_data[664]);
  ch.SetData333(static_cast<int64_t>(raw_data[667]) << 32 | raw_data[666]);
  ch.SetData334(static_cast<int64_t>(raw_data[669]) << 32 | raw_data[668]);
  ch.SetData335(static_cast<int64_t>(raw_data[671]) << 32 | raw_data[670]);
  ch.SetData336(static_cast<int64_t>(raw_data[673]) << 32 | raw_data[672]);
  ch.SetData337(static_cast<int64_t>(raw_data[675]) << 32 | raw_data[674]);
  ch.SetData338(static_cast<int64_t>(raw_data[677]) << 32 | raw_data[676]);
  ch.SetData339(static_cast<int64_t>(raw_data[679]) << 32 | raw_data[678]);
  ch.SetData340(static_cast<int64_t>(raw_data[681]) << 32 | raw_data[680]);
  ch.SetData341(static_cast<int64_t>(raw_data[683]) << 32 | raw_data[682]);
  ch.SetData342(static_cast<int64_t>(raw_data[685]) << 32 | raw_data[684]);
  ch.SetData343(static_cast<int64_t>(raw_data[687]) << 32 | raw_data[686]);
  ch.SetData344(static_cast<int64_t>(raw_data[689]) << 32 | raw_data[688]);
  ch.SetData345(static_cast<int64_t>(raw_data[691]) << 32 | raw_data[690]);
  ch.SetData346(static_cast<int64_t>(raw_data[693]) << 32 | raw_data[692]);
  ch.SetData347(static_cast<int64_t>(raw_data[695]) << 32 | raw_data[694]);
  ch.SetData348(static_cast<int64_t>(raw_data[697]) << 32 | raw_data[696]);
  ch.SetData349(static_cast<int64_t>(raw_data[699]) << 32 | raw_data[698]);
  ch.SetData350(static_cast<int64_t>(raw_data[701]) << 32 | raw_data[700]);
  ch.SetData351(static_cast<int64_t>(raw_data[703]) << 32 | raw_data[702]);
  ch.SetData352(static_cast<int64_t>(raw_data[705]) << 32 | raw_data[704]);
  ch.SetData353(static_cast<int64_t>(raw_data[707]) << 32 | raw_data[706]);
  ch.SetData354(static_cast<int64_t>(raw_data[709]) << 32 | raw_data[708]);
  ch.SetData355(static_cast<int64_t>(raw_data[711]) << 32 | raw_data[710]);
  ch.SetData356(static_cast<int64_t>(raw_data[713]) << 32 | raw_data[712]);
  ch.SetData357(static_cast<int64_t>(raw_data[715]) << 32 | raw_data[714]);
  ch.SetData358(static_cast<int64_t>(raw_data[717]) << 32 | raw_data[716]);
  ch.SetData359(static_cast<int64_t>(raw_data[719]) << 32 | raw_data[718]);
  ch.SetData360(static_cast<int64_t>(raw_data[721]) << 32 | raw_data[720]);
  ch.SetData361(static_cast<int64_t>(raw_data[723]) << 32 | raw_data[722]);
  ch.SetData362(static_cast<int64_t>(raw_data[725]) << 32 | raw_data[724]);
  ch.SetData363(static_cast<int64_t>(raw_data[727]) << 32 | raw_data[726]);
  ch.SetData364(static_cast<int64_t>(raw_data[729]) << 32 | raw_data[728]);
  ch.SetData365(static_cast<int64_t>(raw_data[731]) << 32 | raw_data[730]);
  ch.SetData366(static_cast<int64_t>(raw_data[733]) << 32 | raw_data[732]);
  ch.SetData367(static_cast<int64_t>(raw_data[735]) << 32 | raw_data[734]);
  ch.SetData368(static_cast<int64_t>(raw_data[737]) << 32 | raw_data[736]);
  ch.SetData369(static_cast<int64_t>(raw_data[739]) << 32 | raw_data[738]);
  ch.SetData370(static_cast<int64_t>(raw_data[741]) << 32 | raw_data[740]);
  ch.SetData371(static_cast<int64_t>(raw_data[743]) << 32 | raw_data[742]);
  ch.SetData372(static_cast<int64_t>(raw_data[745]) << 32 | raw_data[744]);
  ch.SetData373(static_cast<int64_t>(raw_data[747]) << 32 | raw_data[746]);
  ch.SetData374(static_cast<int64_t>(raw_data[749]) << 32 | raw_data[748]);
  ch.SetData375(static_cast<int64_t>(raw_data[751]) << 32 | raw_data[750]);
  ch.SetData376(static_cast<int64_t>(raw_data[753]) << 32 | raw_data[752]);
  ch.SetData377(static_cast<int64_t>(raw_data[755]) << 32 | raw_data[754]);
  ch.SetData378(static_cast<int64_t>(raw_data[757]) << 32 | raw_data[756]);
  ch.SetData379(static_cast<int64_t>(raw_data[759]) << 32 | raw_data[758]);
  ch.SetData380(static_cast<int64_t>(raw_data[761]) << 32 | raw_data[760]);
  ch.SetData381(static_cast<int64_t>(raw_data[763]) << 32 | raw_data[762]);
  ch.SetData382(static_cast<int64_t>(raw_data[765]) << 32 | raw_data[764]);
  ch.SetData383(static_cast<int64_t>(raw_data[767]) << 32 | raw_data[766]);
  ch.SetData384(static_cast<int64_t>(raw_data[769]) << 32 | raw_data[768]);
  ch.SetData385(static_cast<int64_t>(raw_data[771]) << 32 | raw_data[770]);
  ch.SetData386(static_cast<int64_t>(raw_data[773]) << 32 | raw_data[772]);
  ch.SetData387(static_cast<int64_t>(raw_data[775]) << 32 | raw_data[774]);
  ch.SetData388(static_cast<int64_t>(raw_data[777]) << 32 | raw_data[776]);
  ch.SetData389(static_cast<int64_t>(raw_data[779]) << 32 | raw_data[778]);
  ch.SetData390(static_cast<int64_t>(raw_data[781]) << 32 | raw_data[780]);
  ch.SetData391(static_cast<int64_t>(raw_data[783]) << 32 | raw_data[782]);
  ch.SetData392(static_cast<int64_t>(raw_data[785]) << 32 | raw_data[784]);
  ch.SetData393(static_cast<int64_t>(raw_data[787]) << 32 | raw_data[786]);
  ch.SetData394(static_cast<int64_t>(raw_data[789]) << 32 | raw_data[788]);
  ch.SetData395(static_cast<int64_t>(raw_data[791]) << 32 | raw_data[790]);
  ch.SetData396(static_cast<int64_t>(raw_data[793]) << 32 | raw_data[792]);
  ch.SetData397(static_cast<int64_t>(raw_data[795]) << 32 | raw_data[794]);
  ch.SetData398(static_cast<int64_t>(raw_data[797]) << 32 | raw_data[796]);
  ch.SetData399(static_cast<int64_t>(raw_data[799]) << 32 | raw_data[798]);
  ch.SetData400(static_cast<int64_t>(raw_data[801]) << 32 | raw_data[800]);
  ch.SetData401(static_cast<int64_t>(raw_data[803]) << 32 | raw_data[802]);
  ch.SetData402(static_cast<int64_t>(raw_data[805]) << 32 | raw_data[804]);
  ch.SetData403(static_cast<int64_t>(raw_data[807]) << 32 | raw_data[806]);
  ch.SetData404(static_cast<int64_t>(raw_data[809]) << 32 | raw_data[808]);
  ch.SetData405(static_cast<int64_t>(raw_data[811]) << 32 | raw_data[810]);
  ch.SetData406(static_cast<int64_t>(raw_data[813]) << 32 | raw_data[812]);
  ch.SetData407(static_cast<int64_t>(raw_data[815]) << 32 | raw_data[814]);
  ch.SetData408(static_cast<int64_t>(raw_data[817]) << 32 | raw_data[816]);
  ch.SetData409(static_cast<int64_t>(raw_data[819]) << 32 | raw_data[818]);
  ch.SetData410(static_cast<int64_t>(raw_data[821]) << 32 | raw_data[820]);
  ch.SetData411(static_cast<int64_t>(raw_data[823]) << 32 | raw_data[822]);
  ch.SetData412(static_cast<int64_t>(raw_data[825]) << 32 | raw_data[824]);
  ch.SetData413(static_cast<int64_t>(raw_data[827]) << 32 | raw_data[826]);
  ch.SetData414(static_cast<int64_t>(raw_data[829]) << 32 | raw_data[828]);
  ch.SetData415(static_cast<int64_t>(raw_data[831]) << 32 | raw_data[830]);
  ch.SetData416(static_cast<int64_t>(raw_data[833]) << 32 | raw_data[832]);
  ch.SetData417(static_cast<int64_t>(raw_data[835]) << 32 | raw_data[834]);
  ch.SetData418(static_cast<int64_t>(raw_data[837]) << 32 | raw_data[836]);
  ch.SetData419(static_cast<int64_t>(raw_data[839]) << 32 | raw_data[838]);
  ch.SetData420(static_cast<int64_t>(raw_data[841]) << 32 | raw_data[840]);
  ch.SetData421(static_cast<int64_t>(raw_data[843]) << 32 | raw_data[842]);
  ch.SetData422(static_cast<int64_t>(raw_data[845]) << 32 | raw_data[844]);
  ch.SetData423(static_cast<int64_t>(raw_data[847]) << 32 | raw_data[846]);
  ch.SetData424(static_cast<int64_t>(raw_data[849]) << 32 | raw_data[848]);
  ch.SetData425(static_cast<int64_t>(raw_data[851]) << 32 | raw_data[850]);
  ch.SetData426(static_cast<int64_t>(raw_data[853]) << 32 | raw_data[852]);
  ch.SetData427(static_cast<int64_t>(raw_data[855]) << 32 | raw_data[854]);
  ch.SetData428(static_cast<int64_t>(raw_data[857]) << 32 | raw_data[856]);
  ch.SetData429(static_cast<int64_t>(raw_data[859]) << 32 | raw_data[858]);
  ch.SetData430(static_cast<int64_t>(raw_data[861]) << 32 | raw_data[860]);
  ch.SetData431(static_cast<int64_t>(raw_data[863]) << 32 | raw_data[862]);
  ch.SetData432(static_cast<int64_t>(raw_data[865]) << 32 | raw_data[864]);
  ch.SetData433(static_cast<int64_t>(raw_data[867]) << 32 | raw_data[866]);
  ch.SetData434(static_cast<int64_t>(raw_data[869]) << 32 | raw_data[868]);
  ch.SetData435(static_cast<int64_t>(raw_data[871]) << 32 | raw_data[870]);
  ch.SetData436(static_cast<int64_t>(raw_data[873]) << 32 | raw_data[872]);
  ch.SetData437(static_cast<int64_t>(raw_data[875]) << 32 | raw_data[874]);
  ch.SetData438(static_cast<int64_t>(raw_data[877]) << 32 | raw_data[876]);
  ch.SetData439(static_cast<int64_t>(raw_data[879]) << 32 | raw_data[878]);
  ch.SetData440(static_cast<int64_t>(raw_data[881]) << 32 | raw_data[880]);
  ch.SetData441(static_cast<int64_t>(raw_data[883]) << 32 | raw_data[882]);
  ch.SetData442(static_cast<int64_t>(raw_data[885]) << 32 | raw_data[884]);
  ch.SetData443(static_cast<int64_t>(raw_data[887]) << 32 | raw_data[886]);
  ch.SetData444(static_cast<int64_t>(raw_data[889]) << 32 | raw_data[888]);
  ch.SetData445(static_cast<int64_t>(raw_data[891]) << 32 | raw_data[890]);
  ch.SetData446(static_cast<int64_t>(raw_data[893]) << 32 | raw_data[892]);
  ch.SetData447(static_cast<int64_t>(raw_data[895]) << 32 | raw_data[894]);
  ch.SetData448(static_cast<int64_t>(raw_data[897]) << 32 | raw_data[896]);
  ch.SetData449(static_cast<int64_t>(raw_data[899]) << 32 | raw_data[898]);
  ch.SetData450(static_cast<int64_t>(raw_data[901]) << 32 | raw_data[900]);
  ch.SetData451(static_cast<int64_t>(raw_data[903]) << 32 | raw_data[902]);
  ch.SetData452(static_cast<int64_t>(raw_data[905]) << 32 | raw_data[904]);
  ch.SetData453(static_cast<int64_t>(raw_data[907]) << 32 | raw_data[906]);
  ch.SetData454(static_cast<int64_t>(raw_data[909]) << 32 | raw_data[908]);
  ch.SetData455(static_cast<int64_t>(raw_data[911]) << 32 | raw_data[910]);
  ch.SetData456(static_cast<int64_t>(raw_data[913]) << 32 | raw_data[912]);
  ch.SetData457(static_cast<int64_t>(raw_data[915]) << 32 | raw_data[914]);
  ch.SetData458(static_cast<int64_t>(raw_data[917]) << 32 | raw_data[916]);
  ch.SetData459(static_cast<int64_t>(raw_data[919]) << 32 | raw_data[918]);
  ch.SetData460(static_cast<int64_t>(raw_data[921]) << 32 | raw_data[920]);
  ch.SetData461(static_cast<int64_t>(raw_data[923]) << 32 | raw_data[922]);
  ch.SetData462(static_cast<int64_t>(raw_data[925]) << 32 | raw_data[924]);
  ch.SetData463(static_cast<int64_t>(raw_data[927]) << 32 | raw_data[926]);
  ch.SetData464(static_cast<int64_t>(raw_data[929]) << 32 | raw_data[928]);
  ch.SetData465(static_cast<int64_t>(raw_data[931]) << 32 | raw_data[930]);
  ch.SetData466(static_cast<int64_t>(raw_data[933]) << 32 | raw_data[932]);
  ch.SetData467(static_cast<int64_t>(raw_data[935]) << 32 | raw_data[934]);
  ch.SetData468(static_cast<int64_t>(raw_data[937]) << 32 | raw_data[936]);
  ch.SetData469(static_cast<int64_t>(raw_data[939]) << 32 | raw_data[938]);
  ch.SetData470(static_cast<int64_t>(raw_data[941]) << 32 | raw_data[940]);
  ch.SetData471(static_cast<int64_t>(raw_data[943]) << 32 | raw_data[942]);
  ch.SetData472(static_cast<int64_t>(raw_data[945]) << 32 | raw_data[944]);
  ch.SetData473(static_cast<int64_t>(raw_data[947]) << 32 | raw_data[946]);
  ch.SetData474(static_cast<int64_t>(raw_data[949]) << 32 | raw_data[948]);
  ch.SetData475(static_cast<int64_t>(raw_data[951]) << 32 | raw_data[950]);
  ch.SetData476(static_cast<int64_t>(raw_data[953]) << 32 | raw_data[952]);
  ch.SetData477(static_cast<int64_t>(raw_data[955]) << 32 | raw_data[954]);
  ch.SetData478(static_cast<int64_t>(raw_data[957]) << 32 | raw_data[956]);
  ch.SetData479(static_cast<int64_t>(raw_data[959]) << 32 | raw_data[958]);
  ch.SetData480(static_cast<int64_t>(raw_data[961]) << 32 | raw_data[960]);
  ch.SetData481(static_cast<int64_t>(raw_data[963]) << 32 | raw_data[962]);
  ch.SetData482(static_cast<int64_t>(raw_data[965]) << 32 | raw_data[964]);
  ch.SetData483(static_cast<int64_t>(raw_data[967]) << 32 | raw_data[966]);
  ch.SetData484(static_cast<int64_t>(raw_data[969]) << 32 | raw_data[968]);
  ch.SetData485(static_cast<int64_t>(raw_data[971]) << 32 | raw_data[970]);
  ch.SetData486(static_cast<int64_t>(raw_data[973]) << 32 | raw_data[972]);
  ch.SetData487(static_cast<int64_t>(raw_data[975]) << 32 | raw_data[974]);
  ch.SetData488(static_cast<int64_t>(raw_data[977]) << 32 | raw_data[976]);
  ch.SetData489(static_cast<int64_t>(raw_data[979]) << 32 | raw_data[978]);
  ch.SetData490(static_cast<int64_t>(raw_data[981]) << 32 | raw_data[980]);
  ch.SetData491(static_cast<int64_t>(raw_data[983]) << 32 | raw_data[982]);
  ch.SetData492(static_cast<int64_t>(raw_data[985]) << 32 | raw_data[984]);
  ch.SetData493(static_cast<int64_t>(raw_data[987]) << 32 | raw_data[986]);
  ch.SetData494(static_cast<int64_t>(raw_data[989]) << 32 | raw_data[988]);
  ch.SetData495(static_cast<int64_t>(raw_data[991]) << 32 | raw_data[990]);
  ch.SetData496(static_cast<int64_t>(raw_data[993]) << 32 | raw_data[992]);
  ch.SetData497(static_cast<int64_t>(raw_data[995]) << 32 | raw_data[994]);
  ch.SetData498(static_cast<int64_t>(raw_data[997]) << 32 | raw_data[996]);
  ch.SetData499(static_cast<int64_t>(raw_data[999]) << 32 | raw_data[998]);
  ch.SetData500(static_cast<int64_t>(raw_data[1001]) << 32 | raw_data[1000]);
  ch.SetData501(static_cast<int64_t>(raw_data[1003]) << 32 | raw_data[1002]);
  ch.SetData502(static_cast<int64_t>(raw_data[1005]) << 32 | raw_data[1004]);
  ch.SetData503(static_cast<int64_t>(raw_data[1007]) << 32 | raw_data[1006]);
  ch.SetData504(static_cast<int64_t>(raw_data[1009]) << 32 | raw_data[1008]);
  ch.SetData505(static_cast<int64_t>(raw_data[1011]) << 32 | raw_data[1010]);
  ch.SetData506(static_cast<int64_t>(raw_data[1013]) << 32 | raw_data[1012]);
  ch.SetData507(static_cast<int64_t>(raw_data[1015]) << 32 | raw_data[1014]);
  ch.SetData508(static_cast<int64_t>(raw_data[1017]) << 32 | raw_data[1016]);
  ch.SetData509(static_cast<int64_t>(raw_data[1019]) << 32 | raw_data[1018]);
  ch.SetData510(static_cast<int64_t>(raw_data[1021]) << 32 | raw_data[1020]);
  ch.SetData511(static_cast<int64_t>(raw_data[1023]) << 32 | raw_data[1022]);
  ch.SetData512(static_cast<int64_t>(raw_data[1025]) << 32 | raw_data[1024]);
  ch.SetData513(static_cast<int64_t>(raw_data[1027]) << 32 | raw_data[1026]);
  ch.SetData514(static_cast<int64_t>(raw_data[1029]) << 32 | raw_data[1028]);
  ch.SetData515(static_cast<int64_t>(raw_data[1031]) << 32 | raw_data[1030]);
  ch.SetData516(static_cast<int64_t>(raw_data[1033]) << 32 | raw_data[1032]);
  ch.SetData517(static_cast<int64_t>(raw_data[1035]) << 32 | raw_data[1034]);
  ch.SetData518(static_cast<int64_t>(raw_data[1037]) << 32 | raw_data[1036]);
  ch.SetData519(static_cast<int64_t>(raw_data[1039]) << 32 | raw_data[1038]);
  ch.SetData520(static_cast<int64_t>(raw_data[1041]) << 32 | raw_data[1040]);
  ch.SetData521(static_cast<int64_t>(raw_data[1043]) << 32 | raw_data[1042]);
  ch.SetData522(static_cast<int64_t>(raw_data[1045]) << 32 | raw_data[1044]);
  ch.SetData523(static_cast<int64_t>(raw_data[1047]) << 32 | raw_data[1046]);
  ch.SetData524(static_cast<int64_t>(raw_data[1049]) << 32 | raw_data[1048]);
  ch.SetData525(static_cast<int64_t>(raw_data[1051]) << 32 | raw_data[1050]);
  ch.SetData526(static_cast<int64_t>(raw_data[1053]) << 32 | raw_data[1052]);
  ch.SetData527(static_cast<int64_t>(raw_data[1055]) << 32 | raw_data[1054]);
  ch.SetData528(static_cast<int64_t>(raw_data[1057]) << 32 | raw_data[1056]);
  ch.SetData529(static_cast<int64_t>(raw_data[1059]) << 32 | raw_data[1058]);
  ch.SetData530(static_cast<int64_t>(raw_data[1061]) << 32 | raw_data[1060]);
  ch.SetData531(static_cast<int64_t>(raw_data[1063]) << 32 | raw_data[1062]);
  ch.SetData532(static_cast<int64_t>(raw_data[1065]) << 32 | raw_data[1064]);
  ch.SetData533(static_cast<int64_t>(raw_data[1067]) << 32 | raw_data[1066]);
  ch.SetData534(static_cast<int64_t>(raw_data[1069]) << 32 | raw_data[1068]);
  ch.SetData535(static_cast<int64_t>(raw_data[1071]) << 32 | raw_data[1070]);
  ch.SetData536(static_cast<int64_t>(raw_data[1073]) << 32 | raw_data[1072]);
  ch.SetData537(static_cast<int64_t>(raw_data[1075]) << 32 | raw_data[1074]);
  ch.SetData538(static_cast<int64_t>(raw_data[1077]) << 32 | raw_data[1076]);
  ch.SetData539(static_cast<int64_t>(raw_data[1079]) << 32 | raw_data[1078]);
  ch.SetData540(static_cast<int64_t>(raw_data[1081]) << 32 | raw_data[1080]);
  ch.SetData541(static_cast<int64_t>(raw_data[1083]) << 32 | raw_data[1082]);
  ch.SetData542(static_cast<int64_t>(raw_data[1085]) << 32 | raw_data[1084]);
  ch.SetData543(static_cast<int64_t>(raw_data[1087]) << 32 | raw_data[1086]);
  ch.SetData544(static_cast<int64_t>(raw_data[1089]) << 32 | raw_data[1088]);
  ch.SetData545(static_cast<int64_t>(raw_data[1091]) << 32 | raw_data[1090]);
  ch.SetData546(static_cast<int64_t>(raw_data[1093]) << 32 | raw_data[1092]);
  ch.SetData547(static_cast<int64_t>(raw_data[1095]) << 32 | raw_data[1094]);
  ch.SetData548(static_cast<int64_t>(raw_data[1097]) << 32 | raw_data[1096]);
  ch.SetData549(static_cast<int64_t>(raw_data[1099]) << 32 | raw_data[1098]);
  ch.SetData550(static_cast<int64_t>(raw_data[1101]) << 32 | raw_data[1100]);
  ch.SetData551(static_cast<int64_t>(raw_data[1103]) << 32 | raw_data[1102]);
  ch.SetData552(static_cast<int64_t>(raw_data[1105]) << 32 | raw_data[1104]);
  ch.SetData553(static_cast<int64_t>(raw_data[1107]) << 32 | raw_data[1106]);
  ch.SetData554(static_cast<int64_t>(raw_data[1109]) << 32 | raw_data[1108]);
  ch.SetData555(static_cast<int64_t>(raw_data[1111]) << 32 | raw_data[1110]);
  ch.SetData556(static_cast<int64_t>(raw_data[1113]) << 32 | raw_data[1112]);
  ch.SetData557(static_cast<int64_t>(raw_data[1115]) << 32 | raw_data[1114]);
  ch.SetData558(static_cast<int64_t>(raw_data[1117]) << 32 | raw_data[1116]);
  ch.SetData559(static_cast<int64_t>(raw_data[1119]) << 32 | raw_data[1118]);
  ch.SetData560(static_cast<int64_t>(raw_data[1121]) << 32 | raw_data[1120]);
  ch.SetData561(static_cast<int64_t>(raw_data[1123]) << 32 | raw_data[1122]);
  ch.SetData562(static_cast<int64_t>(raw_data[1125]) << 32 | raw_data[1124]);
  ch.SetData563(static_cast<int64_t>(raw_data[1127]) << 32 | raw_data[1126]);
  ch.SetData564(static_cast<int64_t>(raw_data[1129]) << 32 | raw_data[1128]);
  ch.SetData565(static_cast<int64_t>(raw_data[1131]) << 32 | raw_data[1130]);
  ch.SetData566(static_cast<int64_t>(raw_data[1133]) << 32 | raw_data[1132]);
  ch.SetData567(static_cast<int64_t>(raw_data[1135]) << 32 | raw_data[1134]);
  ch.SetData568(static_cast<int64_t>(raw_data[1137]) << 32 | raw_data[1136]);
  ch.SetData569(static_cast<int64_t>(raw_data[1139]) << 32 | raw_data[1138]);
  ch.SetData570(static_cast<int64_t>(raw_data[1141]) << 32 | raw_data[1140]);
  ch.SetData571(static_cast<int64_t>(raw_data[1143]) << 32 | raw_data[1142]);
  ch.SetData572(static_cast<int64_t>(raw_data[1145]) << 32 | raw_data[1144]);
  ch.SetData573(static_cast<int64_t>(raw_data[1147]) << 32 | raw_data[1146]);
  ch.SetData574(static_cast<int64_t>(raw_data[1149]) << 32 | raw_data[1148]);
  ch.SetData575(static_cast<int64_t>(raw_data[1151]) << 32 | raw_data[1150]);
  ch.SetData576(static_cast<int64_t>(raw_data[1153]) << 32 | raw_data[1152]);
  ch.SetData577(static_cast<int64_t>(raw_data[1155]) << 32 | raw_data[1154]);
  ch.SetData578(static_cast<int64_t>(raw_data[1157]) << 32 | raw_data[1156]);
  ch.SetData579(static_cast<int64_t>(raw_data[1159]) << 32 | raw_data[1158]);
  ch.SetData580(static_cast<int64_t>(raw_data[1161]) << 32 | raw_data[1160]);
  ch.SetData581(static_cast<int64_t>(raw_data[1163]) << 32 | raw_data[1162]);
  ch.SetData582(static_cast<int64_t>(raw_data[1165]) << 32 | raw_data[1164]);
  ch.SetData583(static_cast<int64_t>(raw_data[1167]) << 32 | raw_data[1166]);
  ch.SetData584(static_cast<int64_t>(raw_data[1169]) << 32 | raw_data[1168]);
  ch.SetData585(static_cast<int64_t>(raw_data[1171]) << 32 | raw_data[1170]);
  ch.SetData586(static_cast<int64_t>(raw_data[1173]) << 32 | raw_data[1172]);
  ch.SetData587(static_cast<int64_t>(raw_data[1175]) << 32 | raw_data[1174]);
  ch.SetData588(static_cast<int64_t>(raw_data[1177]) << 32 | raw_data[1176]);
  ch.SetData589(static_cast<int64_t>(raw_data[1179]) << 32 | raw_data[1178]);
  ch.SetData590(static_cast<int64_t>(raw_data[1181]) << 32 | raw_data[1180]);
  ch.SetData591(static_cast<int64_t>(raw_data[1183]) << 32 | raw_data[1182]);
  ch.SetData592(static_cast<int64_t>(raw_data[1185]) << 32 | raw_data[1184]);
  ch.SetData593(static_cast<int64_t>(raw_data[1187]) << 32 | raw_data[1186]);
  ch.SetData594(static_cast<int64_t>(raw_data[1189]) << 32 | raw_data[1188]);
  ch.SetData595(static_cast<int64_t>(raw_data[1191]) << 32 | raw_data[1190]);
  ch.SetData596(static_cast<int64_t>(raw_data[1193]) << 32 | raw_data[1192]);
  ch.SetData597(static_cast<int64_t>(raw_data[1195]) << 32 | raw_data[1194]);
  ch.SetData598(static_cast<int64_t>(raw_data[1197]) << 32 | raw_data[1196]);
  ch.SetData599(static_cast<int64_t>(raw_data[1199]) << 32 | raw_data[1198]);
  ch.SetData600(static_cast<int64_t>(raw_data[1201]) << 32 | raw_data[1200]);
  ch.SetData601(static_cast<int64_t>(raw_data[1203]) << 32 | raw_data[1202]);
  ch.SetData602(static_cast<int64_t>(raw_data[1205]) << 32 | raw_data[1204]);
  ch.SetData603(static_cast<int64_t>(raw_data[1207]) << 32 | raw_data[1206]);
  ch.SetData604(static_cast<int64_t>(raw_data[1209]) << 32 | raw_data[1208]);
  ch.SetData605(static_cast<int64_t>(raw_data[1211]) << 32 | raw_data[1210]);
  ch.SetData606(static_cast<int64_t>(raw_data[1213]) << 32 | raw_data[1212]);
  ch.SetData607(static_cast<int64_t>(raw_data[1215]) << 32 | raw_data[1214]);
  ch.SetData608(static_cast<int64_t>(raw_data[1217]) << 32 | raw_data[1216]);
  ch.SetData609(static_cast<int64_t>(raw_data[1219]) << 32 | raw_data[1218]);
  ch.SetData610(static_cast<int64_t>(raw_data[1221]) << 32 | raw_data[1220]);
  ch.SetData611(static_cast<int64_t>(raw_data[1223]) << 32 | raw_data[1222]);
  ch.SetData612(static_cast<int64_t>(raw_data[1225]) << 32 | raw_data[1224]);
  ch.SetData613(static_cast<int64_t>(raw_data[1227]) << 32 | raw_data[1226]);
  ch.SetData614(static_cast<int64_t>(raw_data[1229]) << 32 | raw_data[1228]);
  ch.SetData615(static_cast<int64_t>(raw_data[1231]) << 32 | raw_data[1230]);
  ch.SetData616(static_cast<int64_t>(raw_data[1233]) << 32 | raw_data[1232]);
  ch.SetData617(static_cast<int64_t>(raw_data[1235]) << 32 | raw_data[1234]);
  ch.SetData618(static_cast<int64_t>(raw_data[1237]) << 32 | raw_data[1236]);
  ch.SetData619(static_cast<int64_t>(raw_data[1239]) << 32 | raw_data[1238]);
  ch.SetData620(static_cast<int64_t>(raw_data[1241]) << 32 | raw_data[1240]);
  ch.SetData621(static_cast<int64_t>(raw_data[1243]) << 32 | raw_data[1242]);
  ch.SetData622(static_cast<int64_t>(raw_data[1245]) << 32 | raw_data[1244]);
  ch.SetData623(static_cast<int64_t>(raw_data[1247]) << 32 | raw_data[1246]);
  ch.SetData624(static_cast<int64_t>(raw_data[1249]) << 32 | raw_data[1248]);
  ch.SetData625(static_cast<int64_t>(raw_data[1251]) << 32 | raw_data[1250]);
  ch.SetData626(static_cast<int64_t>(raw_data[1253]) << 32 | raw_data[1252]);
  ch.SetData627(static_cast<int64_t>(raw_data[1255]) << 32 | raw_data[1254]);
  ch.SetData628(static_cast<int64_t>(raw_data[1257]) << 32 | raw_data[1256]);
  ch.SetData629(static_cast<int64_t>(raw_data[1259]) << 32 | raw_data[1258]);
  ch.SetData630(static_cast<int64_t>(raw_data[1261]) << 32 | raw_data[1260]);
  ch.SetData631(static_cast<int64_t>(raw_data[1263]) << 32 | raw_data[1262]);
  ch.SetData632(static_cast<int64_t>(raw_data[1265]) << 32 | raw_data[1264]);
  ch.SetData633(static_cast<int64_t>(raw_data[1267]) << 32 | raw_data[1266]);
  ch.SetData634(static_cast<int64_t>(raw_data[1269]) << 32 | raw_data[1268]);
  ch.SetData635(static_cast<int64_t>(raw_data[1271]) << 32 | raw_data[1270]);
  ch.SetData636(static_cast<int64_t>(raw_data[1273]) << 32 | raw_data[1272]);
  ch.SetData637(static_cast<int64_t>(raw_data[1275]) << 32 | raw_data[1274]);
  ch.SetData638(static_cast<int64_t>(raw_data[1277]) << 32 | raw_data[1276]);
  ch.SetData639(static_cast<int64_t>(raw_data[1279]) << 32 | raw_data[1278]);
  ch.SetData640(static_cast<int64_t>(raw_data[1281]) << 32 | raw_data[1280]);
  ch.SetData641(static_cast<int64_t>(raw_data[1283]) << 32 | raw_data[1282]);
  ch.SetData642(static_cast<int64_t>(raw_data[1285]) << 32 | raw_data[1284]);
  ch.SetData643(static_cast<int64_t>(raw_data[1287]) << 32 | raw_data[1286]);
  ch.SetData644(static_cast<int64_t>(raw_data[1289]) << 32 | raw_data[1288]);
  ch.SetData645(static_cast<int64_t>(raw_data[1291]) << 32 | raw_data[1290]);
  ch.SetData646(static_cast<int64_t>(raw_data[1293]) << 32 | raw_data[1292]);
  ch.SetData647(static_cast<int64_t>(raw_data[1295]) << 32 | raw_data[1294]);
  ch.SetData648(static_cast<int64_t>(raw_data[1297]) << 32 | raw_data[1296]);
  ch.SetData649(static_cast<int64_t>(raw_data[1299]) << 32 | raw_data[1298]);
  ch.SetData650(static_cast<int64_t>(raw_data[1301]) << 32 | raw_data[1300]);
  ch.SetData651(static_cast<int64_t>(raw_data[1303]) << 32 | raw_data[1302]);
  ch.SetData652(static_cast<int64_t>(raw_data[1305]) << 32 | raw_data[1304]);
  ch.SetData653(static_cast<int64_t>(raw_data[1307]) << 32 | raw_data[1306]);
  ch.SetData654(static_cast<int64_t>(raw_data[1309]) << 32 | raw_data[1308]);
  ch.SetData655(static_cast<int64_t>(raw_data[1311]) << 32 | raw_data[1310]);
  ch.SetData656(static_cast<int64_t>(raw_data[1313]) << 32 | raw_data[1312]);
  ch.SetData657(static_cast<int64_t>(raw_data[1315]) << 32 | raw_data[1314]);
  ch.SetData658(static_cast<int64_t>(raw_data[1317]) << 32 | raw_data[1316]);
  ch.SetData659(static_cast<int64_t>(raw_data[1319]) << 32 | raw_data[1318]);
  ch.SetData660(static_cast<int64_t>(raw_data[1321]) << 32 | raw_data[1320]);
  ch.SetData661(static_cast<int64_t>(raw_data[1323]) << 32 | raw_data[1322]);
  ch.SetData662(static_cast<int64_t>(raw_data[1325]) << 32 | raw_data[1324]);
  ch.SetData663(static_cast<int64_t>(raw_data[1327]) << 32 | raw_data[1326]);
  ch.SetData664(static_cast<int64_t>(raw_data[1329]) << 32 | raw_data[1328]);
  ch.SetData665(static_cast<int64_t>(raw_data[1331]) << 32 | raw_data[1330]);
  ch.SetData666(static_cast<int64_t>(raw_data[1333]) << 32 | raw_data[1332]);
  ch.SetData667(static_cast<int64_t>(raw_data[1335]) << 32 | raw_data[1334]);
  ch.SetData668(static_cast<int64_t>(raw_data[1337]) << 32 | raw_data[1336]);
  ch.SetData669(static_cast<int64_t>(raw_data[1339]) << 32 | raw_data[1338]);
  ch.SetData670(static_cast<int64_t>(raw_data[1341]) << 32 | raw_data[1340]);
  ch.SetData671(static_cast<int64_t>(raw_data[1343]) << 32 | raw_data[1342]);
  ch.SetData672(static_cast<int64_t>(raw_data[1345]) << 32 | raw_data[1344]);
  ch.SetData673(static_cast<int64_t>(raw_data[1347]) << 32 | raw_data[1346]);
  ch.SetData674(static_cast<int64_t>(raw_data[1349]) << 32 | raw_data[1348]);
  ch.SetData675(static_cast<int64_t>(raw_data[1351]) << 32 | raw_data[1350]);
  ch.SetData676(static_cast<int64_t>(raw_data[1353]) << 32 | raw_data[1352]);
  ch.SetData677(static_cast<int64_t>(raw_data[1355]) << 32 | raw_data[1354]);
  ch.SetData678(static_cast<int64_t>(raw_data[1357]) << 32 | raw_data[1356]);
  ch.SetData679(static_cast<int64_t>(raw_data[1359]) << 32 | raw_data[1358]);
  ch.SetData680(static_cast<int64_t>(raw_data[1361]) << 32 | raw_data[1360]);
  ch.SetData681(static_cast<int64_t>(raw_data[1363]) << 32 | raw_data[1362]);
  ch.SetData682(static_cast<int64_t>(raw_data[1365]) << 32 | raw_data[1364]);
  ch.SetData683(static_cast<int64_t>(raw_data[1367]) << 32 | raw_data[1366]);
  ch.SetData684(static_cast<int64_t>(raw_data[1369]) << 32 | raw_data[1368]);
  ch.SetData685(static_cast<int64_t>(raw_data[1371]) << 32 | raw_data[1370]);
  ch.SetData686(static_cast<int64_t>(raw_data[1373]) << 32 | raw_data[1372]);
  ch.SetData687(static_cast<int64_t>(raw_data[1375]) << 32 | raw_data[1374]);
  ch.SetData688(static_cast<int64_t>(raw_data[1377]) << 32 | raw_data[1376]);
  ch.SetData689(static_cast<int64_t>(raw_data[1379]) << 32 | raw_data[1378]);
  ch.SetData690(static_cast<int64_t>(raw_data[1381]) << 32 | raw_data[1380]);
  ch.SetData691(static_cast<int64_t>(raw_data[1383]) << 32 | raw_data[1382]);
  ch.SetData692(static_cast<int64_t>(raw_data[1385]) << 32 | raw_data[1384]);
  ch.SetData693(static_cast<int64_t>(raw_data[1387]) << 32 | raw_data[1386]);
  ch.SetData694(static_cast<int64_t>(raw_data[1389]) << 32 | raw_data[1388]);
  ch.SetData695(static_cast<int64_t>(raw_data[1391]) << 32 | raw_data[1390]);
  ch.SetData696(static_cast<int64_t>(raw_data[1393]) << 32 | raw_data[1392]);
  ch.SetData697(static_cast<int64_t>(raw_data[1395]) << 32 | raw_data[1394]);
  ch.SetData698(static_cast<int64_t>(raw_data[1397]) << 32 | raw_data[1396]);
  ch.SetData699(static_cast<int64_t>(raw_data[1399]) << 32 | raw_data[1398]);
  ch.SetData700(static_cast<int64_t>(raw_data[1401]) << 32 | raw_data[1400]);
  ch.SetData701(static_cast<int64_t>(raw_data[1403]) << 32 | raw_data[1402]);
  ch.SetData702(static_cast<int64_t>(raw_data[1405]) << 32 | raw_data[1404]);
  ch.SetData703(static_cast<int64_t>(raw_data[1407]) << 32 | raw_data[1406]);
  ch.SetData704(static_cast<int64_t>(raw_data[1409]) << 32 | raw_data[1408]);
  ch.SetData705(static_cast<int64_t>(raw_data[1411]) << 32 | raw_data[1410]);
  ch.SetData706(static_cast<int64_t>(raw_data[1413]) << 32 | raw_data[1412]);
  ch.SetData707(static_cast<int64_t>(raw_data[1415]) << 32 | raw_data[1414]);
  ch.SetData708(static_cast<int64_t>(raw_data[1417]) << 32 | raw_data[1416]);
  ch.SetData709(static_cast<int64_t>(raw_data[1419]) << 32 | raw_data[1418]);
  ch.SetData710(static_cast<int64_t>(raw_data[1421]) << 32 | raw_data[1420]);
  ch.SetData711(static_cast<int64_t>(raw_data[1423]) << 32 | raw_data[1422]);
  ch.SetData712(static_cast<int64_t>(raw_data[1425]) << 32 | raw_data[1424]);
  ch.SetData713(static_cast<int64_t>(raw_data[1427]) << 32 | raw_data[1426]);
  ch.SetData714(static_cast<int64_t>(raw_data[1429]) << 32 | raw_data[1428]);
  ch.SetData715(static_cast<int64_t>(raw_data[1431]) << 32 | raw_data[1430]);
  ch.SetData716(static_cast<int64_t>(raw_data[1433]) << 32 | raw_data[1432]);
  ch.SetData717(static_cast<int64_t>(raw_data[1435]) << 32 | raw_data[1434]);
  ch.SetData718(static_cast<int64_t>(raw_data[1437]) << 32 | raw_data[1436]);
  ch.SetData719(static_cast<int64_t>(raw_data[1439]) << 32 | raw_data[1438]);
  ch.SetData720(static_cast<int64_t>(raw_data[1441]) << 32 | raw_data[1440]);
  ch.SetData721(static_cast<int64_t>(raw_data[1443]) << 32 | raw_data[1442]);
  ch.SetData722(static_cast<int64_t>(raw_data[1445]) << 32 | raw_data[1444]);
  ch.SetData723(static_cast<int64_t>(raw_data[1447]) << 32 | raw_data[1446]);
  ch.SetData724(static_cast<int64_t>(raw_data[1449]) << 32 | raw_data[1448]);
  ch.SetData725(static_cast<int64_t>(raw_data[1451]) << 32 | raw_data[1450]);
  ch.SetData726(static_cast<int64_t>(raw_data[1453]) << 32 | raw_data[1452]);
  ch.SetData727(static_cast<int64_t>(raw_data[1455]) << 32 | raw_data[1454]);
  ch.SetData728(static_cast<int64_t>(raw_data[1457]) << 32 | raw_data[1456]);
  ch.SetData729(static_cast<int64_t>(raw_data[1459]) << 32 | raw_data[1458]);
  ch.SetData730(static_cast<int64_t>(raw_data[1461]) << 32 | raw_data[1460]);
  ch.SetData731(static_cast<int64_t>(raw_data[1463]) << 32 | raw_data[1462]);
  ch.SetData732(static_cast<int64_t>(raw_data[1465]) << 32 | raw_data[1464]);
  ch.SetData733(static_cast<int64_t>(raw_data[1467]) << 32 | raw_data[1466]);
  ch.SetData734(static_cast<int64_t>(raw_data[1469]) << 32 | raw_data[1468]);
  ch.SetData735(static_cast<int64_t>(raw_data[1471]) << 32 | raw_data[1470]);
  ch.SetData736(static_cast<int64_t>(raw_data[1473]) << 32 | raw_data[1472]);
  ch.SetData737(static_cast<int64_t>(raw_data[1475]) << 32 | raw_data[1474]);
  ch.SetData738(static_cast<int64_t>(raw_data[1477]) << 32 | raw_data[1476]);
  ch.SetData739(static_cast<int64_t>(raw_data[1479]) << 32 | raw_data[1478]);
  ch.SetData740(static_cast<int64_t>(raw_data[1481]) << 32 | raw_data[1480]);
  ch.SetData741(static_cast<int64_t>(raw_data[1483]) << 32 | raw_data[1482]);
  ch.SetData742(static_cast<int64_t>(raw_data[1485]) << 32 | raw_data[1484]);
  ch.SetData743(static_cast<int64_t>(raw_data[1487]) << 32 | raw_data[1486]);
  ch.SetData744(static_cast<int64_t>(raw_data[1489]) << 32 | raw_data[1488]);
  ch.SetData745(static_cast<int64_t>(raw_data[1491]) << 32 | raw_data[1490]);
  ch.SetData746(static_cast<int64_t>(raw_data[1493]) << 32 | raw_data[1492]);
  ch.SetData747(static_cast<int64_t>(raw_data[1495]) << 32 | raw_data[1494]);
  ch.SetData748(static_cast<int64_t>(raw_data[1497]) << 32 | raw_data[1496]);
  ch.SetData749(static_cast<int64_t>(raw_data[1499]) << 32 | raw_data[1498]);
  ch.SetData750(static_cast<int64_t>(raw_data[1501]) << 32 | raw_data[1500]);
  ch.SetData751(static_cast<int64_t>(raw_data[1503]) << 32 | raw_data[1502]);
  ch.SetData752(static_cast<int64_t>(raw_data[1505]) << 32 | raw_data[1504]);
  ch.SetData753(static_cast<int64_t>(raw_data[1507]) << 32 | raw_data[1506]);
  ch.SetData754(static_cast<int64_t>(raw_data[1509]) << 32 | raw_data[1508]);
  ch.SetData755(static_cast<int64_t>(raw_data[1511]) << 32 | raw_data[1510]);
  ch.SetData756(static_cast<int64_t>(raw_data[1513]) << 32 | raw_data[1512]);
  ch.SetData757(static_cast<int64_t>(raw_data[1515]) << 32 | raw_data[1514]);
  ch.SetData758(static_cast<int64_t>(raw_data[1517]) << 32 | raw_data[1516]);
  ch.SetData759(static_cast<int64_t>(raw_data[1519]) << 32 | raw_data[1518]);
  ch.SetData760(static_cast<int64_t>(raw_data[1521]) << 32 | raw_data[1520]);
  ch.SetData761(static_cast<int64_t>(raw_data[1523]) << 32 | raw_data[1522]);
  ch.SetData762(static_cast<int64_t>(raw_data[1525]) << 32 | raw_data[1524]);
  ch.SetData763(static_cast<int64_t>(raw_data[1527]) << 32 | raw_data[1526]);
  ch.SetData764(static_cast<int64_t>(raw_data[1529]) << 32 | raw_data[1528]);
  ch.SetData765(static_cast<int64_t>(raw_data[1531]) << 32 | raw_data[1530]);
  ch.SetData766(static_cast<int64_t>(raw_data[1533]) << 32 | raw_data[1532]);
  ch.SetData767(static_cast<int64_t>(raw_data[1535]) << 32 | raw_data[1534]);
  ch.SetData768(static_cast<int64_t>(raw_data[1537]) << 32 | raw_data[1536]);
  ch.SetData769(static_cast<int64_t>(raw_data[1539]) << 32 | raw_data[1538]);
  ch.SetData770(static_cast<int64_t>(raw_data[1541]) << 32 | raw_data[1540]);
  ch.SetData771(static_cast<int64_t>(raw_data[1543]) << 32 | raw_data[1542]);
  ch.SetData772(static_cast<int64_t>(raw_data[1545]) << 32 | raw_data[1544]);
  ch.SetData773(static_cast<int64_t>(raw_data[1547]) << 32 | raw_data[1546]);
  ch.SetData774(static_cast<int64_t>(raw_data[1549]) << 32 | raw_data[1548]);
  ch.SetData775(static_cast<int64_t>(raw_data[1551]) << 32 | raw_data[1550]);
  ch.SetData776(static_cast<int64_t>(raw_data[1553]) << 32 | raw_data[1552]);
  ch.SetData777(static_cast<int64_t>(raw_data[1555]) << 32 | raw_data[1554]);
  ch.SetData778(static_cast<int64_t>(raw_data[1557]) << 32 | raw_data[1556]);
  ch.SetData779(static_cast<int64_t>(raw_data[1559]) << 32 | raw_data[1558]);
  ch.SetData780(static_cast<int64_t>(raw_data[1561]) << 32 | raw_data[1560]);
  ch.SetData781(static_cast<int64_t>(raw_data[1563]) << 32 | raw_data[1562]);
  ch.SetData782(static_cast<int64_t>(raw_data[1565]) << 32 | raw_data[1564]);
  ch.SetData783(static_cast<int64_t>(raw_data[1567]) << 32 | raw_data[1566]);
  ch.SetData784(static_cast<int64_t>(raw_data[1569]) << 32 | raw_data[1568]);
  ch.SetData785(static_cast<int64_t>(raw_data[1571]) << 32 | raw_data[1570]);
  ch.SetData786(static_cast<int64_t>(raw_data[1573]) << 32 | raw_data[1572]);
  ch.SetData787(static_cast<int64_t>(raw_data[1575]) << 32 | raw_data[1574]);
  ch.SetData788(static_cast<int64_t>(raw_data[1577]) << 32 | raw_data[1576]);
  ch.SetData789(static_cast<int64_t>(raw_data[1579]) << 32 | raw_data[1578]);
  ch.SetData790(static_cast<int64_t>(raw_data[1581]) << 32 | raw_data[1580]);
  ch.SetData791(static_cast<int64_t>(raw_data[1583]) << 32 | raw_data[1582]);
  ch.SetData792(static_cast<int64_t>(raw_data[1585]) << 32 | raw_data[1584]);
  ch.SetData793(static_cast<int64_t>(raw_data[1587]) << 32 | raw_data[1586]);
  ch.SetData794(static_cast<int64_t>(raw_data[1589]) << 32 | raw_data[1588]);
  ch.SetData795(static_cast<int64_t>(raw_data[1591]) << 32 | raw_data[1590]);
  ch.SetData796(static_cast<int64_t>(raw_data[1593]) << 32 | raw_data[1592]);
  ch.SetData797(static_cast<int64_t>(raw_data[1595]) << 32 | raw_data[1594]);
  ch.SetData798(static_cast<int64_t>(raw_data[1597]) << 32 | raw_data[1596]);
  ch.SetData799(static_cast<int64_t>(raw_data[1599]) << 32 | raw_data[1598]);
  ch.SetData800(static_cast<int64_t>(raw_data[1601]) << 32 | raw_data[1600]);
  ch.SetData801(static_cast<int64_t>(raw_data[1603]) << 32 | raw_data[1602]);
  ch.SetData802(static_cast<int64_t>(raw_data[1605]) << 32 | raw_data[1604]);
  ch.SetData803(static_cast<int64_t>(raw_data[1607]) << 32 | raw_data[1606]);
  ch.SetData804(static_cast<int64_t>(raw_data[1609]) << 32 | raw_data[1608]);
  ch.SetData805(static_cast<int64_t>(raw_data[1611]) << 32 | raw_data[1610]);
  ch.SetData806(static_cast<int64_t>(raw_data[1613]) << 32 | raw_data[1612]);
  ch.SetData807(static_cast<int64_t>(raw_data[1615]) << 32 | raw_data[1614]);
  ch.SetData808(static_cast<int64_t>(raw_data[1617]) << 32 | raw_data[1616]);
  ch.SetData809(static_cast<int64_t>(raw_data[1619]) << 32 | raw_data[1618]);
  ch.SetData810(static_cast<int64_t>(raw_data[1621]) << 32 | raw_data[1620]);
  ch.SetData811(static_cast<int64_t>(raw_data[1623]) << 32 | raw_data[1622]);
  ch.SetData812(static_cast<int64_t>(raw_data[1625]) << 32 | raw_data[1624]);
  ch.SetData813(static_cast<int64_t>(raw_data[1627]) << 32 | raw_data[1626]);
  ch.SetData814(static_cast<int64_t>(raw_data[1629]) << 32 | raw_data[1628]);
  ch.SetData815(static_cast<int64_t>(raw_data[1631]) << 32 | raw_data[1630]);
  ch.SetData816(static_cast<int64_t>(raw_data[1633]) << 32 | raw_data[1632]);
  ch.SetData817(static_cast<int64_t>(raw_data[1635]) << 32 | raw_data[1634]);
  ch.SetData818(static_cast<int64_t>(raw_data[1637]) << 32 | raw_data[1636]);
  ch.SetData819(static_cast<int64_t>(raw_data[1639]) << 32 | raw_data[1638]);
  ch.SetData820(static_cast<int64_t>(raw_data[1641]) << 32 | raw_data[1640]);
  ch.SetData821(static_cast<int64_t>(raw_data[1643]) << 32 | raw_data[1642]);
  ch.SetData822(static_cast<int64_t>(raw_data[1645]) << 32 | raw_data[1644]);
  ch.SetData823(static_cast<int64_t>(raw_data[1647]) << 32 | raw_data[1646]);
  ch.SetData824(static_cast<int64_t>(raw_data[1649]) << 32 | raw_data[1648]);
  ch.SetData825(static_cast<int64_t>(raw_data[1651]) << 32 | raw_data[1650]);
  ch.SetData826(static_cast<int64_t>(raw_data[1653]) << 32 | raw_data[1652]);
  ch.SetData827(static_cast<int64_t>(raw_data[1655]) << 32 | raw_data[1654]);
  ch.SetData828(static_cast<int64_t>(raw_data[1657]) << 32 | raw_data[1656]);
  ch.SetData829(static_cast<int64_t>(raw_data[1659]) << 32 | raw_data[1658]);
  ch.SetData830(static_cast<int64_t>(raw_data[1661]) << 32 | raw_data[1660]);
  ch.SetData831(static_cast<int64_t>(raw_data[1663]) << 32 | raw_data[1662]);
  ch.SetData832(static_cast<int64_t>(raw_data[1665]) << 32 | raw_data[1664]);
  ch.SetData833(static_cast<int64_t>(raw_data[1667]) << 32 | raw_data[1666]);
  ch.SetData834(static_cast<int64_t>(raw_data[1669]) << 32 | raw_data[1668]);
  ch.SetData835(static_cast<int64_t>(raw_data[1671]) << 32 | raw_data[1670]);
  ch.SetData836(static_cast<int64_t>(raw_data[1673]) << 32 | raw_data[1672]);
  ch.SetData837(static_cast<int64_t>(raw_data[1675]) << 32 | raw_data[1674]);
  ch.SetData838(static_cast<int64_t>(raw_data[1677]) << 32 | raw_data[1676]);
  ch.SetData839(static_cast<int64_t>(raw_data[1679]) << 32 | raw_data[1678]);
  ch.SetData840(static_cast<int64_t>(raw_data[1681]) << 32 | raw_data[1680]);
  ch.SetData841(static_cast<int64_t>(raw_data[1683]) << 32 | raw_data[1682]);
  ch.SetData842(static_cast<int64_t>(raw_data[1685]) << 32 | raw_data[1684]);
  ch.SetData843(static_cast<int64_t>(raw_data[1687]) << 32 | raw_data[1686]);
  ch.SetData844(static_cast<int64_t>(raw_data[1689]) << 32 | raw_data[1688]);
  ch.SetData845(static_cast<int64_t>(raw_data[1691]) << 32 | raw_data[1690]);
  ch.SetData846(static_cast<int64_t>(raw_data[1693]) << 32 | raw_data[1692]);
  ch.SetData847(static_cast<int64_t>(raw_data[1695]) << 32 | raw_data[1694]);
  ch.SetData848(static_cast<int64_t>(raw_data[1697]) << 32 | raw_data[1696]);
  ch.SetData849(static_cast<int64_t>(raw_data[1699]) << 32 | raw_data[1698]);
  ch.SetData850(static_cast<int64_t>(raw_data[1701]) << 32 | raw_data[1700]);
  ch.SetData851(static_cast<int64_t>(raw_data[1703]) << 32 | raw_data[1702]);
  ch.SetData852(static_cast<int64_t>(raw_data[1705]) << 32 | raw_data[1704]);
  ch.SetData853(static_cast<int64_t>(raw_data[1707]) << 32 | raw_data[1706]);
  ch.SetData854(static_cast<int64_t>(raw_data[1709]) << 32 | raw_data[1708]);
  ch.SetData855(static_cast<int64_t>(raw_data[1711]) << 32 | raw_data[1710]);
  ch.SetData856(static_cast<int64_t>(raw_data[1713]) << 32 | raw_data[1712]);
  ch.SetData857(static_cast<int64_t>(raw_data[1715]) << 32 | raw_data[1714]);
  ch.SetData858(static_cast<int64_t>(raw_data[1717]) << 32 | raw_data[1716]);
  ch.SetData859(static_cast<int64_t>(raw_data[1719]) << 32 | raw_data[1718]);
  ch.SetData860(static_cast<int64_t>(raw_data[1721]) << 32 | raw_data[1720]);
  ch.SetData861(static_cast<int64_t>(raw_data[1723]) << 32 | raw_data[1722]);
  ch.SetData862(static_cast<int64_t>(raw_data[1725]) << 32 | raw_data[1724]);
  ch.SetData863(static_cast<int64_t>(raw_data[1727]) << 32 | raw_data[1726]);
  ch.SetData864(static_cast<int64_t>(raw_data[1729]) << 32 | raw_data[1728]);
  ch.SetData865(static_cast<int64_t>(raw_data[1731]) << 32 | raw_data[1730]);
  ch.SetData866(static_cast<int64_t>(raw_data[1733]) << 32 | raw_data[1732]);
  ch.SetData867(static_cast<int64_t>(raw_data[1735]) << 32 | raw_data[1734]);
  ch.SetData868(static_cast<int64_t>(raw_data[1737]) << 32 | raw_data[1736]);
  ch.SetData869(static_cast<int64_t>(raw_data[1739]) << 32 | raw_data[1738]);
  ch.SetData870(static_cast<int64_t>(raw_data[1741]) << 32 | raw_data[1740]);
  ch.SetData871(static_cast<int64_t>(raw_data[1743]) << 32 | raw_data[1742]);
  ch.SetData872(static_cast<int64_t>(raw_data[1745]) << 32 | raw_data[1744]);
  ch.SetData873(static_cast<int64_t>(raw_data[1747]) << 32 | raw_data[1746]);
  ch.SetData874(static_cast<int64_t>(raw_data[1749]) << 32 | raw_data[1748]);
  ch.SetData875(static_cast<int64_t>(raw_data[1751]) << 32 | raw_data[1750]);
  ch.SetData876(static_cast<int64_t>(raw_data[1753]) << 32 | raw_data[1752]);
  ch.SetData877(static_cast<int64_t>(raw_data[1755]) << 32 | raw_data[1754]);
  ch.SetData878(static_cast<int64_t>(raw_data[1757]) << 32 | raw_data[1756]);
  ch.SetData879(static_cast<int64_t>(raw_data[1759]) << 32 | raw_data[1758]);
  ch.SetData880(static_cast<int64_t>(raw_data[1761]) << 32 | raw_data[1760]);
  ch.SetData881(static_cast<int64_t>(raw_data[1763]) << 32 | raw_data[1762]);
  ch.SetData882(static_cast<int64_t>(raw_data[1765]) << 32 | raw_data[1764]);
  ch.SetData883(static_cast<int64_t>(raw_data[1767]) << 32 | raw_data[1766]);
  ch.SetData884(static_cast<int64_t>(raw_data[1769]) << 32 | raw_data[1768]);
  ch.SetData885(static_cast<int64_t>(raw_data[1771]) << 32 | raw_data[1770]);
  ch.SetData886(static_cast<int64_t>(raw_data[1773]) << 32 | raw_data[1772]);
  ch.SetData887(static_cast<int64_t>(raw_data[1775]) << 32 | raw_data[1774]);
  ch.SetData888(static_cast<int64_t>(raw_data[1777]) << 32 | raw_data[1776]);
  ch.SetData889(static_cast<int64_t>(raw_data[1779]) << 32 | raw_data[1778]);
  ch.SetData890(static_cast<int64_t>(raw_data[1781]) << 32 | raw_data[1780]);
  ch.SetData891(static_cast<int64_t>(raw_data[1783]) << 32 | raw_data[1782]);
  ch.SetData892(static_cast<int64_t>(raw_data[1785]) << 32 | raw_data[1784]);
  ch.SetData893(static_cast<int64_t>(raw_data[1787]) << 32 | raw_data[1786]);
  ch.SetData894(static_cast<int64_t>(raw_data[1789]) << 32 | raw_data[1788]);
  ch.SetData895(static_cast<int64_t>(raw_data[1791]) << 32 | raw_data[1790]);
  ch.SetData896(static_cast<int64_t>(raw_data[1793]) << 32 | raw_data[1792]);
  ch.SetData897(static_cast<int64_t>(raw_data[1795]) << 32 | raw_data[1794]);
  ch.SetData898(static_cast<int64_t>(raw_data[1797]) << 32 | raw_data[1796]);
  ch.SetData899(static_cast<int64_t>(raw_data[1799]) << 32 | raw_data[1798]);
  ch.SetData900(static_cast<int64_t>(raw_data[1801]) << 32 | raw_data[1800]);
  ch.SetData901(static_cast<int64_t>(raw_data[1803]) << 32 | raw_data[1802]);
  ch.SetData902(static_cast<int64_t>(raw_data[1805]) << 32 | raw_data[1804]);
  ch.SetData903(static_cast<int64_t>(raw_data[1807]) << 32 | raw_data[1806]);
  ch.SetData904(static_cast<int64_t>(raw_data[1809]) << 32 | raw_data[1808]);
  ch.SetData905(static_cast<int64_t>(raw_data[1811]) << 32 | raw_data[1810]);
  ch.SetData906(static_cast<int64_t>(raw_data[1813]) << 32 | raw_data[1812]);
  ch.SetData907(static_cast<int64_t>(raw_data[1815]) << 32 | raw_data[1814]);
  ch.SetData908(static_cast<int64_t>(raw_data[1817]) << 32 | raw_data[1816]);
  ch.SetData909(static_cast<int64_t>(raw_data[1819]) << 32 | raw_data[1818]);
  ch.SetData910(static_cast<int64_t>(raw_data[1821]) << 32 | raw_data[1820]);
  ch.SetData911(static_cast<int64_t>(raw_data[1823]) << 32 | raw_data[1822]);
  ch.SetData912(static_cast<int64_t>(raw_data[1825]) << 32 | raw_data[1824]);
  ch.SetData913(static_cast<int64_t>(raw_data[1827]) << 32 | raw_data[1826]);
  ch.SetData914(static_cast<int64_t>(raw_data[1829]) << 32 | raw_data[1828]);
  ch.SetData915(static_cast<int64_t>(raw_data[1831]) << 32 | raw_data[1830]);
  ch.SetData916(static_cast<int64_t>(raw_data[1833]) << 32 | raw_data[1832]);
  ch.SetData917(static_cast<int64_t>(raw_data[1835]) << 32 | raw_data[1834]);
  ch.SetData918(static_cast<int64_t>(raw_data[1837]) << 32 | raw_data[1836]);
  ch.SetData919(static_cast<int64_t>(raw_data[1839]) << 32 | raw_data[1838]);
  ch.SetData920(static_cast<int64_t>(raw_data[1841]) << 32 | raw_data[1840]);
  ch.SetData921(static_cast<int64_t>(raw_data[1843]) << 32 | raw_data[1842]);
  ch.SetData922(static_cast<int64_t>(raw_data[1845]) << 32 | raw_data[1844]);
  ch.SetData923(static_cast<int64_t>(raw_data[1847]) << 32 | raw_data[1846]);
  ch.SetData924(static_cast<int64_t>(raw_data[1849]) << 32 | raw_data[1848]);
  ch.SetData925(static_cast<int64_t>(raw_data[1851]) << 32 | raw_data[1850]);
  ch.SetData926(static_cast<int64_t>(raw_data[1853]) << 32 | raw_data[1852]);
  ch.SetData927(static_cast<int64_t>(raw_data[1855]) << 32 | raw_data[1854]);
  ch.SetData928(static_cast<int64_t>(raw_data[1857]) << 32 | raw_data[1856]);
  ch.SetData929(static_cast<int64_t>(raw_data[1859]) << 32 | raw_data[1858]);
  ch.SetData930(static_cast<int64_t>(raw_data[1861]) << 32 | raw_data[1860]);
  ch.SetData931(static_cast<int64_t>(raw_data[1863]) << 32 | raw_data[1862]);
  ch.SetData932(static_cast<int64_t>(raw_data[1865]) << 32 | raw_data[1864]);
  ch.SetData933(static_cast<int64_t>(raw_data[1867]) << 32 | raw_data[1866]);
  ch.SetData934(static_cast<int64_t>(raw_data[1869]) << 32 | raw_data[1868]);
  ch.SetData935(static_cast<int64_t>(raw_data[1871]) << 32 | raw_data[1870]);
  ch.SetData936(static_cast<int64_t>(raw_data[1873]) << 32 | raw_data[1872]);
  ch.SetData937(static_cast<int64_t>(raw_data[1875]) << 32 | raw_data[1874]);
  ch.SetData938(static_cast<int64_t>(raw_data[1877]) << 32 | raw_data[1876]);
  ch.SetData939(static_cast<int64_t>(raw_data[1879]) << 32 | raw_data[1878]);
  ch.SetData940(static_cast<int64_t>(raw_data[1881]) << 32 | raw_data[1880]);
  ch.SetData941(static_cast<int64_t>(raw_data[1883]) << 32 | raw_data[1882]);
  ch.SetData942(static_cast<int64_t>(raw_data[1885]) << 32 | raw_data[1884]);
  ch.SetData943(static_cast<int64_t>(raw_data[1887]) << 32 | raw_data[1886]);
  ch.SetData944(static_cast<int64_t>(raw_data[1889]) << 32 | raw_data[1888]);
  ch.SetData945(static_cast<int64_t>(raw_data[1891]) << 32 | raw_data[1890]);
  ch.SetData946(static_cast<int64_t>(raw_data[1893]) << 32 | raw_data[1892]);
  ch.SetData947(static_cast<int64_t>(raw_data[1895]) << 32 | raw_data[1894]);
  ch.SetData948(static_cast<int64_t>(raw_data[1897]) << 32 | raw_data[1896]);
  ch.SetData949(static_cast<int64_t>(raw_data[1899]) << 32 | raw_data[1898]);
  ch.SetData950(static_cast<int64_t>(raw_data[1901]) << 32 | raw_data[1900]);
  ch.SetData951(static_cast<int64_t>(raw_data[1903]) << 32 | raw_data[1902]);
  ch.SetData952(static_cast<int64_t>(raw_data[1905]) << 32 | raw_data[1904]);
  ch.SetData953(static_cast<int64_t>(raw_data[1907]) << 32 | raw_data[1906]);
  ch.SetData954(static_cast<int64_t>(raw_data[1909]) << 32 | raw_data[1908]);
  ch.SetData955(static_cast<int64_t>(raw_data[1911]) << 32 | raw_data[1910]);
  ch.SetData956(static_cast<int64_t>(raw_data[1913]) << 32 | raw_data[1912]);
  ch.SetData957(static_cast<int64_t>(raw_data[1915]) << 32 | raw_data[1914]);
  ch.SetData958(static_cast<int64_t>(raw_data[1917]) << 32 | raw_data[1916]);
  ch.SetData959(static_cast<int64_t>(raw_data[1919]) << 32 | raw_data[1918]);
  ch.SetData960(static_cast<int64_t>(raw_data[1921]) << 32 | raw_data[1920]);
  ch.SetData961(static_cast<int64_t>(raw_data[1923]) << 32 | raw_data[1922]);
  ch.SetData962(static_cast<int64_t>(raw_data[1925]) << 32 | raw_data[1924]);
  ch.SetData963(static_cast<int64_t>(raw_data[1927]) << 32 | raw_data[1926]);
  ch.SetData964(static_cast<int64_t>(raw_data[1929]) << 32 | raw_data[1928]);
  ch.SetData965(static_cast<int64_t>(raw_data[1931]) << 32 | raw_data[1930]);
  ch.SetData966(static_cast<int64_t>(raw_data[1933]) << 32 | raw_data[1932]);
  ch.SetData967(static_cast<int64_t>(raw_data[1935]) << 32 | raw_data[1934]);
  ch.SetData968(static_cast<int64_t>(raw_data[1937]) << 32 | raw_data[1936]);
  ch.SetData969(static_cast<int64_t>(raw_data[1939]) << 32 | raw_data[1938]);
  ch.SetData970(static_cast<int64_t>(raw_data[1941]) << 32 | raw_data[1940]);
  ch.SetData971(static_cast<int64_t>(raw_data[1943]) << 32 | raw_data[1942]);
  ch.SetData972(static_cast<int64_t>(raw_data[1945]) << 32 | raw_data[1944]);
  ch.SetData973(static_cast<int64_t>(raw_data[1947]) << 32 | raw_data[1946]);
  ch.SetData974(static_cast<int64_t>(raw_data[1949]) << 32 | raw_data[1948]);
  ch.SetData975(static_cast<int64_t>(raw_data[1951]) << 32 | raw_data[1950]);
  ch.SetData976(static_cast<int64_t>(raw_data[1953]) << 32 | raw_data[1952]);
  ch.SetData977(static_cast<int64_t>(raw_data[1955]) << 32 | raw_data[1954]);
  ch.SetData978(static_cast<int64_t>(raw_data[1957]) << 32 | raw_data[1956]);
  ch.SetData979(static_cast<int64_t>(raw_data[1959]) << 32 | raw_data[1958]);
  ch.SetData980(static_cast<int64_t>(raw_data[1961]) << 32 | raw_data[1960]);
  ch.SetData981(static_cast<int64_t>(raw_data[1963]) << 32 | raw_data[1962]);
  ch.SetData982(static_cast<int64_t>(raw_data[1965]) << 32 | raw_data[1964]);
  ch.SetData983(static_cast<int64_t>(raw_data[1967]) << 32 | raw_data[1966]);
  ch.SetData984(static_cast<int64_t>(raw_data[1969]) << 32 | raw_data[1968]);
  ch.SetData985(static_cast<int64_t>(raw_data[1971]) << 32 | raw_data[1970]);
  ch.SetData986(static_cast<int64_t>(raw_data[1973]) << 32 | raw_data[1972]);
  ch.SetData987(static_cast<int64_t>(raw_data[1975]) << 32 | raw_data[1974]);
  ch.SetData988(static_cast<int64_t>(raw_data[1977]) << 32 | raw_data[1976]);
  ch.SetData989(static_cast<int64_t>(raw_data[1979]) << 32 | raw_data[1978]);
  ch.SetData990(static_cast<int64_t>(raw_data[1981]) << 32 | raw_data[1980]);
  ch.SetData991(static_cast<int64_t>(raw_data[1983]) << 32 | raw_data[1982]);
  ch.SetData992(static_cast<int64_t>(raw_data[1985]) << 32 | raw_data[1984]);
  ch.SetData993(static_cast<int64_t>(raw_data[1987]) << 32 | raw_data[1986]);
  ch.SetData994(static_cast<int64_t>(raw_data[1989]) << 32 | raw_data[1988]);
  ch.SetData995(static_cast<int64_t>(raw_data[1991]) << 32 | raw_data[1990]);
  ch.SetData996(static_cast<int64_t>(raw_data[1993]) << 32 | raw_data[1992]);
  ch.SetData997(static_cast<int64_t>(raw_data[1995]) << 32 | raw_data[1994]);
  ch.SetData998(static_cast<int64_t>(raw_data[1997]) << 32 | raw_data[1996]);
  ch.SetData999(static_cast<int64_t>(raw_data[1999]) << 32 | raw_data[1998]);
  ch.SetData1000(static_cast<int64_t>(raw_data[2001]) << 32 | raw_data[2000]);
  ch.SetData1001(static_cast<int64_t>(raw_data[2003]) << 32 | raw_data[2002]);
  ch.SetData1002(static_cast<int64_t>(raw_data[2005]) << 32 | raw_data[2004]);
  ch.SetData1003(static_cast<int64_t>(raw_data[2007]) << 32 | raw_data[2006]);
  ch.SetData1004(static_cast<int64_t>(raw_data[2009]) << 32 | raw_data[2008]);
  ch.SetData1005(static_cast<int64_t>(raw_data[2011]) << 32 | raw_data[2010]);
  ch.SetData1006(static_cast<int64_t>(raw_data[2013]) << 32 | raw_data[2012]);
  ch.SetData1007(static_cast<int64_t>(raw_data[2015]) << 32 | raw_data[2014]);
  ch.SetData1008(static_cast<int64_t>(raw_data[2017]) << 32 | raw_data[2016]);
  ch.SetData1009(static_cast<int64_t>(raw_data[2019]) << 32 | raw_data[2018]);
  ch.SetData1010(static_cast<int64_t>(raw_data[2021]) << 32 | raw_data[2020]);
  ch.SetData1011(static_cast<int64_t>(raw_data[2023]) << 32 | raw_data[2022]);
  ch.SetData1012(static_cast<int64_t>(raw_data[2025]) << 32 | raw_data[2024]);
  ch.SetData1013(static_cast<int64_t>(raw_data[2027]) << 32 | raw_data[2026]);
  ch.SetData1014(static_cast<int64_t>(raw_data[2029]) << 32 | raw_data[2028]);
  ch.SetData1015(static_cast<int64_t>(raw_data[2031]) << 32 | raw_data[2030]);
  ch.SetData1016(static_cast<int64_t>(raw_data[2033]) << 32 | raw_data[2032]);
  ch.SetData1017(static_cast<int64_t>(raw_data[2035]) << 32 | raw_data[2034]);
  ch.SetData1018(static_cast<int64_t>(raw_data[2037]) << 32 | raw_data[2036]);
  ch.SetData1019(static_cast<int64_t>(raw_data[2039]) << 32 | raw_data[2038]);
  ch.SetData1020(static_cast<int64_t>(raw_data[2041]) << 32 | raw_data[2040]);
  ch.SetData1021(static_cast<int64_t>(raw_data[2043]) << 32 | raw_data[2042]);
  ch.SetData1022(static_cast<int64_t>(raw_data[2045]) << 32 | raw_data[2044]);
  ch.SetData1023(static_cast<int64_t>(raw_data[2047]) << 32 | raw_data[2046]);
  ch.Record(ukm_recorder);
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
  for (int i = 0; i < bitsInUnsigned; ++i) {
    if (i > 0) {
      mask <<= 1;
    }
    double random = base::RandDouble();
    if (random < kProduceCompileHintsNoiseLevel / 2) {
      // Change this bit.
      mask |= 1;
    }
  }

  *data = *data ^ mask;
}

}  // namespace blink::v8_compile_hints

#endif  // BUILDFLAG(PRODUCE_V8_COMPILE_HINTS)
