/*
 * Copyright 2018 The Error Prone Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.chromium.third_party.errorprone;

import static com.google.errorprone.BugPattern.LinkType.CUSTOM;
import static com.google.errorprone.BugPattern.SeverityLevel.WARNING;
import static com.google.errorprone.util.ASTHelpers.canBeRemoved;
import static com.google.errorprone.util.ASTHelpers.getSymbol;
import static com.google.errorprone.util.ASTHelpers.hasAnnotation;
import static com.google.errorprone.util.ASTHelpers.stripParentheses;
import static com.google.errorprone.util.AnnotationNames.VAR_ANNOTATION;
import static com.google.errorprone.util.MoreAnnotations.asElement;

import com.google.common.collect.ImmutableSet;
import com.google.common.collect.Sets;
import com.google.errorprone.BugPattern;
import com.google.errorprone.ErrorProneFlags;
import com.google.errorprone.VisitorState;
import com.google.errorprone.bugpatterns.BugChecker;
import com.google.errorprone.bugpatterns.BugChecker.CompilationUnitTreeMatcher;
import com.google.errorprone.bugpatterns.WellKnownKeep;
import com.google.errorprone.fixes.SuggestedFixes;
import com.google.errorprone.matchers.Description;
import com.google.errorprone.matchers.InjectMatchers;
import com.sun.source.tree.AssignmentTree;
import com.sun.source.tree.BlockTree;
import com.sun.source.tree.ClassTree;
import com.sun.source.tree.CompilationUnitTree;
import com.sun.source.tree.CompoundAssignmentTree;
import com.sun.source.tree.ExpressionStatementTree;
import com.sun.source.tree.ExpressionTree;
import com.sun.source.tree.IdentifierTree;
import com.sun.source.tree.LambdaExpressionTree;
import com.sun.source.tree.MemberSelectTree;
import com.sun.source.tree.MethodInvocationTree;
import com.sun.source.tree.MethodTree;
import com.sun.source.tree.ReturnTree;
import com.sun.source.tree.StatementTree;
import com.sun.source.tree.Tree;
import com.sun.source.tree.Tree.Kind;
import com.sun.source.tree.TryTree;
import com.sun.source.tree.UnaryTree;
import com.sun.source.tree.VariableTree;
import com.sun.source.util.TreePathScanner;
import com.sun.source.util.TreeScanner;
import com.sun.tools.javac.code.Flags;
import com.sun.tools.javac.code.Symbol;
import com.sun.tools.javac.code.Symbol.MethodSymbol;
import com.sun.tools.javac.code.Symbol.TypeSymbol;
import com.sun.tools.javac.code.Symbol.VarSymbol;

import org.chromium.build.annotations.ServiceImpl;

import java.util.ArrayList;
import java.util.Collection;
import java.util.EnumSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import javax.inject.Inject;
import javax.lang.model.element.AnnotationMirror;
import javax.lang.model.element.ElementKind;
import javax.lang.model.element.Modifier;

/**
 * A temporary copy of Error Prone's FieldCanBeFinal check, which avoids calling the slow
 * SuggestedFixes.compilesWithFix(). Delete this check once Error Prone's upstream check picks up
 * this fix. See README.chromium.
 *
 * <p>It does nothing unless -XepOpt:FieldCanBeFinalFast:Enabled=true is passed. This allows
 * errorprone.py to enable it without naming it via -Xep (which fails when using an Error Prone
 * native-image that does not yet contain this check), and stops it from running when
 * -Xep:FieldCanBeFinal enables it via its altName. The "FieldCanBeFinal" altName makes
 * {@code @SuppressWarnings("FieldCanBeFinal")} suppress this check as well.
 *
 * @author Liam Miller-Cushon (cushon@google.com)
 */
@ServiceImpl(BugChecker.class)
@BugPattern(
        name = "FieldCanBeFinalFast",
        altNames = "FieldCanBeFinal",
        summary = "This field is only assigned during initialization; consider making it final",
        severity = WARNING,
        linkType = CUSTOM,
        link = "https://errorprone.info/bugpattern/FieldCanBeFinal")
public class FieldCanBeFinalFast extends BugChecker implements CompilationUnitTreeMatcher {

    /** Annotations that imply a field is non-constant. */
    // TODO(cushon): consider supporting @Var as a meta-annotation
    private static final ImmutableSet<String> IMPLICIT_VAR_ANNOTATIONS =
            ImmutableSet.<String>builder()
                    .addAll(InjectMatchers.INJECT_ANNOTATIONS)
                    .add(
                            // keep-sorted start
                            "com.beust.jcommander.Parameter",
                            "com.google.common.annotations.NonFinalForGwt",
                            "com.google.gwt.uibinder.client.UiField",
                            "com.google.inject.testing.fieldbinder.Bind",
                            "com.google.testing.junit.testparameterinjector.TestParameter",
                            "jakarta.jdo.annotations.Persistent",
                            "jakarta.persistence.Id",
                            "jakarta.xml.bind.annotation.XmlAttribute",
                            "javax.jdo.annotations.Persistent",
                            "javax.persistence.Id",
                            "javax.xml.bind.annotation.XmlAttribute",
                            "org.kohsuke.args4j.Argument",
                            "org.kohsuke.args4j.Option",
                            "org.mockito.Spy",
                            "picocli.CommandLine.Option",
                            VAR_ANNOTATION
                            // keep-sorted end
                            )
                    .build();

    private static final String OBJECTIFY_PREFIX = "com.googlecode.objectify.";

    /**
     * Annotations that imply a field is non-constant, and that do not have a canonical
     * implementation. Instead, we match on any annotation with one of the following simple names.
     */
    private static final ImmutableSet<String> IMPLICIT_VAR_ANNOTATION_SIMPLE_NAMES =
            ImmutableSet.of("NonFinalForTesting", "NotFinalForTesting");

    /** Unary operator kinds that implicitly assign to their operand. */
    private static final ImmutableSet<Kind> UNARY_ASSIGNMENT =
            Sets.immutableEnumSet(
                    Kind.PREFIX_DECREMENT,
                    Kind.POSTFIX_DECREMENT,
                    Kind.PREFIX_INCREMENT,
                    Kind.POSTFIX_INCREMENT);

    private final WellKnownKeep wellKnownKeep;
    private final boolean enabled;

    /**
     * Required by ServiceLoader, which Error Prone uses to find plugin checks. Error Prone creates
     * instances using the {@code @Inject} constructor.
     */
    public FieldCanBeFinalFast() {
        throw new UnsupportedOperationException();
    }

    @Inject
    public FieldCanBeFinalFast(WellKnownKeep wellKnownKeep, ErrorProneFlags flags) {
        this.wellKnownKeep = wellKnownKeep;
        this.enabled = flags.getBoolean("FieldCanBeFinalFast:Enabled").orElse(false);
    }

    /** The initialization context where an assignment occurred. */
    private enum InitializationContext {
        /** A class (static) initializer. */
        STATIC,
        /** An instance initializer. */
        INSTANCE,
        /** Neither a static or instance initializer. */
        NONE
    }

    /** A record of all assignments to variables in the current compilation unit. */
    private static class VariableAssignmentRecords {

        private final Map<VarSymbol, VariableAssignments> assignments = new LinkedHashMap<>();

        /** Returns all {@link VariableAssignments} in the current compilation unit. */
        private Collection<VariableAssignments> getAssignments() {
            return assignments.values();
        }

        /** Records an assignment to a variable. */
        private void recordAssignment(Tree tree, InitializationContext init) {
            Symbol sym = getSymbol(tree);
            if (sym != null && sym.getKind() == ElementKind.FIELD) {
                recordAssignment((VarSymbol) sym, init);
            }
        }

        /** Records an assignment to a variable. */
        private void recordAssignment(VarSymbol sym, InitializationContext init) {
            getDeclaration(sym).recordAssignment(init);
        }

        private VariableAssignments getDeclaration(VarSymbol sym) {
            return assignments.computeIfAbsent(sym, VariableAssignments::new);
        }

        /** Records a variable declaration. */
        private void recordDeclaration(VarSymbol sym, VariableTree tree, ClassTree enclosingClass) {
            getDeclaration(sym).recordDeclaration(tree, enclosingClass);
        }
    }

    /** A record of all assignments to a specific variable in the current compilation unit. */
    private static class VariableAssignments {

        private final VarSymbol sym;
        private final EnumSet<InitializationContext> writes =
                EnumSet.noneOf(InitializationContext.class);
        private int totalWrites;
        private VariableTree declaration;
        private ClassTree enclosingClass;

        VariableAssignments(VarSymbol sym) {
            this.sym = sym;
        }

        /** Records an assignment to the variable. */
        private void recordAssignment(InitializationContext init) {
            writes.add(init);
            totalWrites++;
        }

        /** Records that a variable was declared in this compilation unit. */
        private void recordDeclaration(VariableTree tree, ClassTree enclosingClass) {
            this.declaration = tree;
            this.enclosingClass = enclosingClass;
        }

        /** Returns true if the variable is effectively final. */
        private boolean isEffectivelyFinal() {
            if (declaration == null) {
                return false;
            }
            if (sym.getModifiers().contains(Modifier.FINAL)) {
                // actually final != effectively final
                return false;
            }
            if (writes.contains(InitializationContext.NONE)) {
                return false;
            }
            // Fast pre-filter before definitelyCanBeFinal: check that the field is initialized at
            // least once in an initializer with the right static-ness. Multiple initializations
            // are allowed here because we don't consider control flow yet, and zero
            // initializations are allowed to handle class and instance initializers, and
            // delegating constructors that don't initialize the field directly.
            InitializationContext wanted;
            InitializationContext other;
            if (sym.isStatic()) {
                wanted = InitializationContext.STATIC;
                other = InitializationContext.INSTANCE;
            } else {
                wanted = InitializationContext.INSTANCE;
                other = InitializationContext.STATIC;
            }
            if (writes.contains(other)) {
                return false;
            }
            return writes.contains(wanted) || (sym.flags() & Flags.HASINIT) == Flags.HASINIT;
        }

        private VariableTree declaration() {
            return declaration;
        }
    }

    @Override
    public Description matchCompilationUnit(CompilationUnitTree tree, VisitorState state) {
        if (!enabled) {
            return Description.NO_MATCH;
        }
        VariableAssignmentRecords writes = new VariableAssignmentRecords();
        new FinalScanner(writes, state).scan(state.getPath(), InitializationContext.NONE);
        for (VariableAssignments var : writes.getAssignments()) {
            if (!var.isEffectivelyFinal()) {
                continue;
            }
            if (!canBeRemoved(var.sym)) {
                continue;
            }
            if (wellKnownKeep.shouldKeep(var.declaration)) {
                continue;
            }
            if (IMPLICIT_VAR_ANNOTATIONS.stream().anyMatch(a -> hasAnnotation(var.sym, a, state))) {
                continue;
            }
            for (AnnotationMirror anno : var.sym.getAnnotationMirrors()) {
                TypeSymbol annoElement = asElement(anno);
                if (IMPLICIT_VAR_ANNOTATION_SIMPLE_NAMES.contains(
                        annoElement.getSimpleName().toString())) {
                    return Description.NO_MATCH;
                }
                if (annoElement.getQualifiedName().toString().startsWith(OBJECTIFY_PREFIX)) {
                    return Description.NO_MATCH;
                }
            }
            if (!definitelyCanBeFinal(var)) {
                continue;
            }
            VariableTree varDecl = var.declaration();
            SuggestedFixes.addModifiers(varDecl, state, Modifier.FINAL)
                    .ifPresent(f -> state.reportMatch(describeMatch(varDecl, f)));
        }
        return Description.NO_MATCH;
    }

    /**
     * Returns true if adding {@code final} to the field is guaranteed to compile, checking that the
     * field is either initialized at its declaration with no other writes, or initialized once in
     * straight-line code before any reads or early returns.
     */
    private static boolean definitelyCanBeFinal(VariableAssignments var) {
        VarSymbol sym = var.sym;
        if (sym.getModifiers().contains(Modifier.VOLATILE)) {
            return false;
        }
        if (var.declaration.getInitializer() != null && var.totalWrites > 0) {
            return false;
        }
        for (Tree member : var.enclosingClass.getMembers()) {
            if (member instanceof VariableTree variable
                    && variable != var.declaration
                    && variable.getModifiers() == var.declaration.getModifiers()) {
                // javac shares a single JCModifiers instance across declarators in a multi-variable
                // declaration (e.g. `int a, b;`), where adding `final` would affect all of them.
                return false;
            }
        }
        if (var.declaration.getInitializer() != null) {
            return true;
        }
        boolean isStatic = sym.isStatic();
        List<StatementTree> initializerStatements = new ArrayList<>();
        List<BlockTree> constructorBodies = new ArrayList<>();
        for (Tree member : var.enclosingClass.getMembers()) {
            switch (member) {
                case BlockTree block when block.isStatic() == isStatic ->
                        initializerStatements.addAll(block.getStatements());
                case VariableTree variable
                        when variable != var.declaration
                                && getSymbol(variable).isStatic() == isStatic
                                && variable.getInitializer() != null -> {
                    if (mayRead(variable.getInitializer(), sym)) {
                        return false;
                    }
                }
                case MethodTree method
                        when !isStatic
                                && getSymbol(method).isConstructor()
                                && method.getBody() != null ->
                        constructorBodies.add(method.getBody());
                default -> {}
            }
        }
        if (isStatic) {
            return var.totalWrites == 1 && isInitializedInStraightLine(initializerStatements, sym);
        }
        if (var.totalWrites == 1 && isInitializedInStraightLine(initializerStatements, sym)) {
            return true;
        }
        if (initializerStatements.stream().anyMatch(s -> mayRead(s, sym))) {
            return false;
        }
        int nonDelegatingConstructors = 0;
        for (BlockTree body : constructorBodies) {
            if (isDelegatingConstructor(body)) {
                continue;
            }
            nonDelegatingConstructors++;
            if (!isInitializedInStraightLine(body.getStatements(), sym)) {
                return false;
            }
        }
        return nonDelegatingConstructors > 0 && var.totalWrites == nonDelegatingConstructors;
    }

    /**
     * Returns true if {@code statements} definitely initializes {@code sym} in straight-line code
     * before any reads or early returns.
     *
     * <p>Note that {@link #mayRead} also visits the left-hand side of assignments, so any write to
     * {@code sym} other than a top-level {@code sym = ...;} statement (such as {@code if (c) sym =
     * 1;} or {@code foo(sym = 1)}) is treated as a read and causes this method to return {@code
     * false}. Combined with checking {@code totalWrites == nonDelegatingConstructors}, this ensures
     * the field is assigned on every path and never reassigned.
     */
    private static boolean isInitializedInStraightLine(
            List<? extends StatementTree> statements, VarSymbol sym) {
        for (StatementTree statement : statements) {
            switch (statement) {
                case ExpressionStatementTree exprStmt
                        when exprStmt.getExpression() instanceof AssignmentTree assignment
                                && sym.equals(getSymbol(assignment.getVariable())) -> {
                    return isSimpleFieldAccess(assignment.getVariable(), sym)
                            && !mayRead(assignment.getExpression(), sym);
                }
                case BlockTree block -> {
                    if (isInitializedInStraightLine(block.getStatements(), sym)) {
                        return true;
                    }
                }
                case TryTree tryTree
                        when tryTree.getCatches().isEmpty()
                                && tryTree.getResources().stream().noneMatch(r -> mayRead(r, sym))
                                && (tryTree.getFinallyBlock() == null
                                        || (!mayRead(tryTree.getFinallyBlock(), sym)
                                                && !containsReturn(tryTree.getFinallyBlock()))) -> {
                    if (isInitializedInStraightLine(tryTree.getBlock().getStatements(), sym)) {
                        return true;
                    }
                }
                default -> {}
            }
            if (mayRead(statement, sym) || containsReturn(statement)) {
                return false;
            }
        }
        return false;
    }

    private static boolean isSimpleFieldAccess(ExpressionTree tree, VarSymbol sym) {
        return sym.isStatic() ? tree instanceof IdentifierTree : isThisAccess(tree);
    }

    /**
     * Returns true if {@code tree} is an unqualified identifier or a {@code this.x} member select.
     * Callers are expected to check that {@code tree} resolves to the relevant field symbol.
     */
    private static boolean isThisAccess(Tree tree) {
        return switch (tree) {
            case IdentifierTree ident -> true;
            case MemberSelectTree memberSelect ->
                    memberSelect.getExpression() instanceof IdentifierTree ident
                            && ident.getName().contentEquals("this");
            default -> false;
        };
    }

    private static boolean isDelegatingConstructor(BlockTree body) {
        return body.getStatements().stream().anyMatch(FieldCanBeFinalFast::isThisCall);
    }

    private static boolean isThisCall(StatementTree statement) {
        return statement instanceof ExpressionStatementTree exprStmt
                && exprStmt.getExpression() instanceof MethodInvocationTree invocation
                && invocation.getMethodSelect() instanceof IdentifierTree identifier
                && identifier.getName().contentEquals("this");
    }

    /**
     * Returns true if the tree may read the variable in a way that is subject to the definite
     * assignment rules, i.e. using its simple name or qualified by {@code this}. Reads via other
     * instances, and reads within the methods of nested classes, don't need the variable to be
     * definitely assigned. (Field initializers and initializer blocks of nested classes are still
     * scanned, since the compiler checks those in anonymous classes.)
     */
    private static boolean mayRead(Tree tree, VarSymbol sym) {
        boolean[] result = {false};
        new TreeScanner<Void, Void>() {
            @Override
            public Void visitIdentifier(IdentifierTree node, Void unused) {
                if (sym.equals(getSymbol(node))) {
                    result[0] = true;
                }
                return super.visitIdentifier(node, null);
            }

            @Override
            public Void visitMemberSelect(MemberSelectTree node, Void unused) {
                if (sym.equals(getSymbol(node)) && isThis(node.getExpression())) {
                    result[0] = true;
                }
                return super.visitMemberSelect(node, null);
            }

            @Override
            public Void visitClass(ClassTree node, Void unused) {
                for (Tree member : node.getMembers()) {
                    switch (member) {
                        case VariableTree variable -> scan(variable.getInitializer(), null);
                        case BlockTree block -> scan(block, null);
                        case ClassTree nested -> scan(nested, null);
                        default -> {}
                    }
                }
                return null;
            }
        }.scan(tree, null);
        return result[0];
    }

    /** Returns true for {@code this} and {@code Outer.this}, possibly parenthesized. */
    private static boolean isThis(ExpressionTree tree) {
        return switch (stripParentheses(tree)) {
            case IdentifierTree identifier -> identifier.getName().contentEquals("this");
            case MemberSelectTree memberSelect ->
                    memberSelect.getIdentifier().contentEquals("this");
            default -> false;
        };
    }

    private static boolean containsReturn(Tree tree) {
        boolean[] result = {false};
        new TreeScanner<Void, Void>() {
            @Override
            public Void visitReturn(ReturnTree node, Void unused) {
                result[0] = true;
                return null;
            }

            @Override
            public Void visitLambdaExpression(LambdaExpressionTree node, Void unused) {
                return null;
            }

            @Override
            public Void visitClass(ClassTree node, Void unused) {
                return null;
            }
        }.scan(tree, null);
        return result[0];
    }

    /** Record assignments to possibly-final variables in a compilation unit. */
    private final class FinalScanner extends TreePathScanner<Void, InitializationContext> {

        private final VariableAssignmentRecords writes;
        private final VisitorState compilationState;
        private boolean classSuppressed;

        private FinalScanner(VariableAssignmentRecords writes, VisitorState compilationState) {
            this.writes = writes;
            this.compilationState = compilationState;
        }

        @Override
        public Void visitVariable(VariableTree node, InitializationContext init) {
            VarSymbol sym = getSymbol(node);
            if (!classSuppressed
                    && sym.getKind() == ElementKind.FIELD
                    && !isSuppressed(node, compilationState)) {
                writes.recordDeclaration(
                        sym, node, (ClassTree) getCurrentPath().getParentPath().getLeaf());
            }
            return super.visitVariable(node, InitializationContext.NONE);
        }

        @Override
        public Void visitLambdaExpression(
                LambdaExpressionTree lambdaExpressionTree, InitializationContext init) {
            // reset the initialization context when entering lambda
            return super.visitLambdaExpression(lambdaExpressionTree, InitializationContext.NONE);
        }

        @Override
        public Void visitBlock(BlockTree node, InitializationContext init) {
            if (getCurrentPath().getParentPath().getLeaf().getKind() == Kind.CLASS) {
                init =
                        node.isStatic()
                                ? InitializationContext.STATIC
                                : InitializationContext.INSTANCE;
            }
            return super.visitBlock(node, init);
        }

        @Override
        public Void visitMethod(MethodTree node, InitializationContext init) {
            MethodSymbol sym = getSymbol(node);
            if (sym.isConstructor()) {
                init = InitializationContext.INSTANCE;
            }
            return super.visitMethod(node, init);
        }

        @Override
        public Void visitAssignment(AssignmentTree node, InitializationContext init) {
            if (init == InitializationContext.INSTANCE && !isThisAccess(node.getVariable())) {
                // don't record assignments in initializers that aren't to members of the object
                // being initialized
                init = InitializationContext.NONE;
            }
            writes.recordAssignment(node.getVariable(), init);
            return super.visitAssignment(node, init);
        }

        @Override
        public Void visitClass(ClassTree node, InitializationContext init) {
            boolean prevSuppressed = classSuppressed;
            classSuppressed = classSuppressed || isClassSuppressed(node);
            try {
                // reset the initialization context when entering a new declaration
                return super.visitClass(node, InitializationContext.NONE);
            } finally {
                classSuppressed = prevSuppressed;
            }
        }

        private boolean isClassSuppressed(ClassTree node) {
            VisitorState state = compilationState.withPath(getCurrentPath());

            if (isSuppressed(node, state)) {
                return true;
            }

            for (AnnotationMirror anno : getSymbol(node).getAnnotationMirrors()) {
                TypeSymbol annoElement = asElement(anno);
                if (annoElement.getQualifiedName().toString().startsWith(OBJECTIFY_PREFIX)) {
                    return true;
                }
            }
            return false;
        }

        @Override
        public Void visitCompoundAssignment(
                CompoundAssignmentTree node, InitializationContext init) {
            init = InitializationContext.NONE;
            writes.recordAssignment(node.getVariable(), init);
            return super.visitCompoundAssignment(node, init);
        }

        @Override
        public Void visitUnary(UnaryTree node, InitializationContext init) {
            if (UNARY_ASSIGNMENT.contains(node.getKind())) {
                init = InitializationContext.NONE;
                writes.recordAssignment(node.getExpression(), init);
            }
            return super.visitUnary(node, init);
        }
    }
}
