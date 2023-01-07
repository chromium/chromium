#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import subprocess
import sys

_GROUP_ID = 'com.android.tools.build'
_ARTIFACT_ID = 'manifest-merger'
_FINAL_NAME = 'manifest-merger.jar'

_POM_TEMPLATE = """\
<project>
  <modelVersion>4.0.0</modelVersion>
  <groupId>group</groupId>
  <artifactId>artifact</artifactId>
  <version>1</version>
  <dependencies>
    <dependency>
      <groupId>{group_id}</groupId>
      <artifactId>{artifact_id}</artifactId>
      <version>{version}</version>
      <scope>runtime</scope>
    </dependency>
  </dependencies>
  <build>
    <plugins>
      <plugin>
        <artifactId>maven-assembly-plugin</artifactId>
        <version>3.3.0</version>
        <configuration>
          <descriptorRefs>
            <descriptorRef>jar-with-dependencies</descriptorRef>
          </descriptorRefs>
        </configuration>
        <executions>
          <execution>
            <phase>package</phase>
            <goals>
              <goal>single</goal>
            </goals>
          </execution>
        </executions>
      </plugin>
    </plugins>
  </build>
  <repositories>
    <repository>
      <id>google</id>
      <name>google</name>
      <url>https://maven.google.com/</url>
    </repository>
  </repositories>
</project>
"""


def main(output_prefix: str, deps_prefix: str):
    # Remove the patch version at the end: 30.4.0-alpha05.cr0 => 30.4.0-alpha05
    version = os.environ['_3PP_VERSION'].rsplit('.', 1)[0]
    with open('pom.xml', 'w') as f:
        f.write(
            _POM_TEMPLATE.format(version=version,
                                 group_id=_GROUP_ID,
                                 artifact_id=_ARTIFACT_ID))

    # Set up JAVA_HOME for the mvn command to find the JDK.
    env = os.environ.copy()
    env['JAVA_HOME'] = os.path.join(deps_prefix, 'current')

    # Ensure that mvn works and the environment is set up correctly.
    subprocess.run(['mvn', '-v'], check=True, env=env)

    # Build the jar file, explicitly specify -f to reduce sources of error.
    subprocess.run(['mvn', 'clean', 'assembly:single', '-f', 'pom.xml'],
                   check=True,
                   env=env)

    # Move and rename output to the upload directory. Moving only the jar avoids
    # polluting the output directory with maven intermediate outputs.
    os.makedirs(output_prefix, exist_ok=True)
    shutil.move('target/artifact-1-jar-with-dependencies.jar',
                os.path.join(output_prefix, _FINAL_NAME))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
