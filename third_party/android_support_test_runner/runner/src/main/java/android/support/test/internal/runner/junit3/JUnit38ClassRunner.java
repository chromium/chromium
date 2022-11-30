package android.support.test.internal.runner.junit3;

import java.lang.annotation.Annotation;
import java.lang.reflect.Method;
import junit.extensions.TestDecorator;
import junit.framework.AssertionFailedError;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestListener;
import junit.framework.TestResult;
import junit.framework.TestSuite;
import org.junit.runner.Describable;
import org.junit.runner.Description;
import org.junit.runner.Runner;
import org.junit.runner.manipulation.Filter;
import org.junit.runner.manipulation.Filterable;
import org.junit.runner.manipulation.NoTestsRemainException;
import org.junit.runner.manipulation.Sortable;
import org.junit.runner.manipulation.Sorter;
import org.junit.runner.notification.Failure;
import org.junit.runner.notification.RunNotifier;

// this is a virtually identical copy of org.junit.internal.runner.JUnit38ClassRunner
// from junit 4.12-snapshot. Copied here so android.support.test can work
// with JUnit 4.10+ and pick up the annotation filtering fixes made in 4.12.
public class JUnit38ClassRunner extends Runner implements Filterable, Sortable {
    private static final class OldTestClassAdaptingListener implements
    TestListener {
        private final RunNotifier fNotifier;

        private OldTestClassAdaptingListener(RunNotifier notifier) {
            fNotifier = notifier;
        }

        @Override
        public void endTest(Test test) {
            fNotifier.fireTestFinished(asDescription(test));
        }

        @Override
        public void startTest(Test test) {
            fNotifier.fireTestStarted(asDescription(test));
        }

        // Implement junit.framework.TestListener
        @Override
        public void addError(Test test, Throwable t) {
            Failure failure = new Failure(asDescription(test), t);
            fNotifier.fireTestFailure(failure);
        }

        private Description asDescription(Test test) {
            if (test instanceof Describable) {
                Describable facade = (Describable) test;
                return facade.getDescription();
            }
            if (test instanceof TestCase) {
                return makeDescription(test);
            }
            return Description.createTestDescription(getEffectiveClass(test), test.toString());
        }

        private Class<? extends Test> getEffectiveClass(Test test) {
            return test.getClass();
        }

        @Override
        public void addFailure(Test test, AssertionFailedError t) {
            addError(test, t);
        }
    }

    private volatile Test fTest;

    public JUnit38ClassRunner(Class<?> klass) {
        this(new TestSuite(klass.asSubclass(TestCase.class)));
    }

    public JUnit38ClassRunner(Test test) {
        super();
        setTest(test);
    }

    @Override
    public void run(RunNotifier notifier) {
        TestResult result = new TestResult();
        result.addListener(createAdaptingListener(notifier));
        getTest().run(result);
    }

    public TestListener createAdaptingListener(final RunNotifier notifier) {
        return new OldTestClassAdaptingListener(notifier);
    }

    @Override
    public Description getDescription() {
        return makeDescription(getTest());
    }

    // android-changed - change from private so it can be accessed when filtering AndroidTestSuites
    static Description makeDescription(Test test) {
        if (test instanceof TestCase) {
            TestCase tc = (TestCase) test;
            return Description.createTestDescription(tc.getClass(), tc.getName(),
                    getAnnotations(tc));
        } else if (test instanceof TestSuite) {
            TestSuite ts = (TestSuite) test;
            String name = ts.getName() == null ? createSuiteDescription(ts) : ts.getName();
            Description description = Description.createSuiteDescription(name);
            int n = ts.testCount();
            for (int i = 0; i < n; i++) {
                Description made = makeDescription(ts.testAt(i));
                description.addChild(made);
            }
            return description;
        } else if (test instanceof Describable) {
            Describable adapter = (Describable) test;
            return adapter.getDescription();
        } else if (test instanceof TestDecorator) {
            TestDecorator decorator = (TestDecorator) test;
            return makeDescription(decorator.getTest());
        } else {
            // This is the best we can do in this case
            return Description.createSuiteDescription(test.getClass());
        }
    }

    /**
     * Get the annotations associated with given TestCase.
     * @param test the TestCase.
     */
    private static Annotation[] getAnnotations(TestCase test) {
        try {
            Method m = test.getClass().getMethod(test.getName());
            return m.getDeclaredAnnotations();
        } catch (SecurityException e) {
        } catch (NoSuchMethodException e) {
        }
        return new Annotation[0];
    }

    private static String createSuiteDescription(TestSuite ts) {
        int count = ts.countTestCases();
        String example = count == 0 ? "" : String.format(" [example: %s]", ts.testAt(0));
        return String.format("TestSuite with %s tests%s", count, example);
    }

    @Override
    public void filter(Filter filter) throws NoTestsRemainException {
        if (getTest() instanceof Filterable) {
            Filterable adapter = (Filterable) getTest();
            adapter.filter(filter);
        } else if (getTest() instanceof TestSuite) {
            TestSuite suite = (TestSuite) getTest();
            TestSuite filtered = new TestSuite(suite.getName());
            int n = suite.testCount();
            for (int i = 0; i < n; i++) {
                Test test = suite.testAt(i);
                if (filter.shouldRun(makeDescription(test))) {
                    filtered.addTest(test);
                }
            }
            setTest(filtered);
            if (filtered.testCount() == 0) {
                throw new NoTestsRemainException();
            }
        }
    }

    @Override
    public void sort(Sorter sorter) {
        if (getTest() instanceof Sortable) {
            Sortable adapter = (Sortable) getTest();
            adapter.sort(sorter);
        }
    }

    private void setTest(Test test) {
        fTest = test;
    }

    private Test getTest() {
        return fTest;
    }
}
