# Android Lint Playground

This directory contains a standalone environment to run and experiment with Android Lint in Chromium.

## How to Run

1.  Make edits to `Playground.java` to include the code you want to test.
2.  Run the lint script:
    ```bash
    ./lint.sh
    ```

## Testing Custom Lint Rules from AARs

Some lint rules are defined as custom checks within AAR libraries. By default, this standalone playground setup does not automatically load these custom checks.

To test issues involving these rules:

1.  **Extract the custom lint jar**:
    Custom lint rules are often stored as `lint.jar` inside the `.aar` file. You can extract it using `unzip`:
    ```bash
    unzip -p path/to/library.aar lint.jar > custom_lint_rules.jar
    ```
2.  **Load the rule in lint**:
    Modify `lint.sh` to include the `--lint-rule-jars` flag pointing to the extracted jar:
    ```bash
    $JAVA_PATH -cp $LINT_PATH com.android.tools.lint.Main --project ./project.xml --lint-rule-jars ./custom_lint_rules.jar
    ```

## Classpath Dependencies

Certain lint rules require dependent classes to be present in the classpath to function correctly, even if the playground file doesn't directly use them. This is often necessary for type resolution when the rule analyzes inheritance or method signatures involving classes from external libraries.

To add dependencies to the classpath:
1.  Extract `classes.jar` from the relevant AAR.
2.  Add it to `project.xml` using the `<classpath>` tag:
    ```xml
    <classpath file="path/to/extracted_classes.jar"/>
    ```

## Note on Full Build Integration

In full build systems (like GN/Ninja in Chromium), custom lint rules from AARs might not be automatically extracted and loaded by default unless the build wrapper is configured to do so. If a rule is known to exist in an AAR but is not triggering during a full build, the build wrapper might need to be updated to extract and pass these rules to lint similarly to how it's done manually here.
