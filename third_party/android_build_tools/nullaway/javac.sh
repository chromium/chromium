#!/bin/bash
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to help repro NullAway bugs.
# cd out/Debug
# ../../third_party/android_build_tools/nullaway/javac.sh Foo.java

# To test with tip-of-tree:
# cd out
# git clone https://github.com/uber/NullAway.git
# cd NullAway
# JAVA_HOME=../../third_party/jdk/current/ ./gradlew jar
local_jar=../NullAway/nullaway/build/libs/nullaway.jar

if [[ ! -f obj/tools/android/errorprone_plugin/errorprone_plugin.javac.jar ]]; then
  echo "Do a build and run script from your output directory"
  exit 1
fi


# Command based on running an __errorprone compile_java.py step and adding
# --print-javac-command-line.
exec ../../third_party/jdk/current/bin/javac -g --release 17 \
    -encoding UTF-8 -sourcepath : -Xlint:-dep-ann -Xlint:-removal -J-XX:+PerfDisableSharedMem -J--add-exports=jdk.compiler/com.sun.tools.javac.api=ALL-UNNAMED -J--add-exports=jdk.compiler/com.sun.tools.javac.file=ALL-UNNAMED -J--add-exports=jdk.compiler/com.sun.tools.javac.main=ALL-UNNAMED -J--add-exports=jdk.compiler/com.sun.tools.javac.model=ALL-UNNAMED -J--add-exports=jdk.compiler/com.sun.tools.javac.parser=ALL-UNNAMED -J--add-exports=jdk.compiler/com.sun.tools.javac.processing=ALL-UNNAMED -J--add-exports=jdk.compiler/com.sun.tools.javac.tree=ALL-UNNAMED -J--add-exports=jdk.compiler/com.sun.tools.javac.util=ALL-UNNAMED -J--add-opens=jdk.compiler/com.sun.tools.javac.code=ALL-UNNAMED -J--add-opens=jdk.compiler/com.sun.tools.javac.comp=ALL-UNNAMED -XDcompilePolicy=simple '-Xplugin:ErrorProne -XepOpt:NullAway:AnnotatedPackages= -XepOpt:NullAway:CustomContractAnnotations=org.chromium.build.annotations.Contract -XepOpt:NullAway:CheckContracts=true -XepOpt:NullAway:CastToNonNullMethod=org.chromium.build.NullUtil.assumeNonNull -XepOpt:NullAway:AssertsEnabled=true -XepOpt:NullAway:AcknowledgeRestrictiveAnnotations=true -XepOpt:Nullaway:AcknowledgeAndroidRecent=true -XepOpt:NullAway:JSpecifyMode=true -XepOpt:NullAway:KnownInitializers=android.app.Application.onCreate,android.app.Activity.onCreate,android.app.Service.onCreate,android.app.backup.BackupAgent.onCreate,android.content.ContentProvider.attachInfo,android.content.ContentProvider.onCreate,android.content.ContentWrapper.attachBaseContext -XepAllErrorsAsWarnings -XepDisableWarningsInGeneratedCode -Xep:InlineMeInliner:OFF -Xep:InlineMeSuggester:OFF -Xep:HidingField:OFF -Xep:AlreadyChecked:OFF -Xep:DirectInvocationOnMock:OFF -Xep:MockNotUsedInProduction:OFF -Xep:JdkObsolete:OFF -Xep:ReturnValueIgnored:OFF -Xep:StaticAssignmentInConstructor:OFF -Xep:InvalidBlockTag:OFF -Xep:InvalidParam:OFF -Xep:InvalidLink:OFF -Xep:InvalidInlineTag:OFF -Xep:MalformedInlineTag:OFF -Xep:MissingSummary:OFF -Xep:UnescapedEntity:OFF -Xep:UnrecognisedJavadocTag:OFF -Xep:MutablePublicArray:OFF -Xep:NonCanonicalType:OFF -Xep:DoNotClaimAnnotations:OFF -Xep:JavaUtilDate:OFF -Xep:IdentityHashMapUsage:OFF -Xep:StaticMockMember:OFF -Xep:StaticAssignmentOfThrowable:OFF -Xep:CatchAndPrintStackTrace:OFF -Xep:TypeParameterUnusedInFormals:OFF -Xep:DefaultCharset:OFF -Xep:FutureReturnValueIgnored:OFF -Xep:ThreadJoinLoop:OFF -Xep:StringSplitter:OFF -Xep:ClassNewInstance:OFF -Xep:ThreadLocalUsage:OFF -Xep:EqualsHashCode:OFF -Xep:OverrideThrowableToString:OFF -Xep:UnsafeReflectiveConstructionCast:OFF -Xep:MixedMutabilityReturnType:OFF -Xep:EqualsGetClass:OFF -Xep:UndefinedEquals:OFF -Xep:SameNameButDifferent:OFF -Xep:UnnecessaryLambda:OFF -Xep:EmptyCatch:OFF -Xep:BadImport:OFF -Xep:UseCorrectAssertInTests:OFF -Xep:RefersToDaggerCodegen:OFF -Xep:RemoveUnusedImports:OFF -Xep:UnicodeEscape:OFF -Xep:NonApiType:OFF -Xep:StringCharset:OFF -Xep:StringCaseLocaleUsage:OFF -Xep:RedundantControlFlow:OFF -Xep:BinderIdentityRestoredDangerously:WARN -Xep:EmptyIf:WARN -Xep:EqualsBrokenForNull:WARN -Xep:InvalidThrows:WARN -Xep:LongLiteralLowerCaseSuffix:WARN -Xep:MultiVariableDeclaration:WARN -Xep:RedundantOverride:WARN -Xep:StaticQualifiedUsingExpression:WARN -Xep:TimeUnitMismatch:WARN -Xep:UnnecessaryStaticImport:WARN -Xep:UseBinds:WARN -Xep:WildcardImport:WARN -Xep:NoStreams:WARN' -XDshould-stop.ifError=FLOW -proc:none -processorpath obj/tools/android/errorprone_plugin/errorprone_plugin.javac.jar:obj/build/android/build_java.javac.jar:../../third_party/android_build_tools/error_prone/cipd/error_prone_core.jar:../../third_party/android_build_tools/error_prone_javac/cipd/javac.jar:$local_jar:../../third_party/android_build_tools/nullaway/cipd/nullaway.jar \
    -classpath ../../third_party/android_deps/cipd/libs/org_jspecify_jspecify/jspecify-1.0.0.jar:obj/build/android/build_java.javac.jar:..:. \
    "$@"
