// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import com.oracle.svm.core.annotate.Substitute;
import com.oracle.svm.core.annotate.TargetClass;

import org.jetbrains.kotlin.analysis.low.level.api.fir.api.targets.LLPartialBodyAnalysisState;
import org.jetbrains.kotlin.analysis.low.level.api.fir.util.LLPhaseEventCompleter;
import org.jetbrains.kotlin.analysis.low.level.api.fir.util.LLPhaseSuspensionEventCompleter;
import org.jetbrains.kotlin.fir.FirElementWithResolveState;
import org.jetbrains.kotlin.fir.declarations.FirResolvePhase;

import java.util.List;

/**
 * Kotlin's LLFlightRecorder looks up JFR event types in its static initializer, which fails in an
 * image without JFR, and building with --enable-monitoring=jfr crashes the PGO-instrumented build.
 * Every method returns early unless a JFR recording enables its event, so replace the class with
 * one where no event is ever enabled. In a substituted class only members marked @Substitute
 * replace the original ones.
 */
@TargetClass(className = "org.jetbrains.kotlin.analysis.low.level.api.fir.util.LLFlightRecorder")
@Substitute
final class Target_LLFlightRecorder {
    @Substitute
    public static final Target_LLFlightRecorder INSTANCE = new Target_LLFlightRecorder();

    @Substitute
    public LLPhaseEventCompleter phase$org_jetbrains_kotlin_low_level_api_fir(
            FirElementWithResolveState target,
            List<?> containingDeclarations,
            FirResolvePhase phase) {
        return null;
    }

    @Substitute
    public void partialBodyAnalyzed$org_jetbrains_kotlin_low_level_api_fir(
            FirElementWithResolveState declaration, LLPartialBodyAnalysisState state) {}

    @Substitute
    public void readyPhase$org_jetbrains_kotlin_low_level_api_fir(
            FirElementWithResolveState target, FirResolvePhase requestedPhase) {}

    @Substitute
    public void readyPhase$org_jetbrains_kotlin_low_level_api_fir(
            FirElementWithResolveState target,
            List<?> containingDeclarations,
            FirResolvePhase requestedPhase) {}

    @Substitute
    public LLPhaseSuspensionEventCompleter phaseSuspension$org_jetbrains_kotlin_low_level_api_fir(
            FirElementWithResolveState declaration, FirResolvePhase requestedPhase) {
        return null;
    }

    @Substitute
    public void stopWorldSessionInvalidationScheduled() {}

    @Substitute
    public void stopWorldSessionInvalidationComplete() {}
}
